/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/services/nus.h>
#include "conn_time_sync.h"
#include <bluetooth/hci_vs_sdc.h>

/* Connection interval, in units of 1.25 ms (BT_LE_CONN_PARAM's own units).
 * Change only this to sweep the sync frequency; everything below derives
 * from it and stays valid for any 6 <= N <= 3200 (the spec's own bounds).
 */
//#define CONN_INTERVAL_UNITS 3200
#define CONN_INTERVAL_US    (CONN_INTERVAL_UNITS * 1250)

/* Same N, but interpreted in units of 10 ms for the supervision timeout:
 * timeout_ms = 10 * N is always 8x the interval (1.25 * N), comfortably
 * above the 2x the Bluetooth spec requires (timeout_ms > 2 * interval_ms).
 * Below N=10 that would put the timeout under the spec's own 100 ms floor
 * for this parameter (independent of the interval), so clamp it there.
 */
#define CONN_TIMEOUT_UNITS ((CONN_INTERVAL_UNITS) > 10 ? (CONN_INTERVAL_UNITS) : 10)

/* Must stay above the connection interval, otherwise the toggle command is
 * already stale by the time it reaches the peripheral (see peripheral.c's
 * "controller_time_us + 500 < trigger_time_local" check), and below
 * LED_TOGGLE_PERIOD_MS, otherwise the central re-arms its own pending
 * trigger before it ever fires.
 *
 * A pure 2x/4x-the-interval margin collapses to almost nothing at small N
 * (e.g. 7.5 ms of offset slack at N=6), which isn't enough to absorb the
 * scheduling jitter of servicing multiple simultaneous peripherals - one
 * missed connection event for that link and the command is already late.
 * The original sample's fixed values at its 10 ms default interval -
 * 50 ms offset, 120 ms period - are known to work, so floor both exactly
 * there regardless of N, and only let them grow past that (2x/4x the
 * interval) once N is large enough that the proportional value exceeds it.
 */
#define LED_TOGGLE_TIME_OFFSET_US \
	((CONN_INTERVAL_US * 2) > 50000 ? (CONN_INTERVAL_US * 2) : 50000)
#define LED_TOGGLE_PERIOD_MS \
	((CONN_INTERVAL_US * 4 / 1000) > 120 ? (CONN_INTERVAL_US * 4 / 1000) : 120)

#define ADV_NAME_STR_MAX_LEN (sizeof(CONFIG_BT_DEVICE_NAME))

static void scan_start(void);

static bool led_value;
static uint8_t volatile conn_count;

static const struct bt_uuid *timed_char_uuid = BT_UUID_TIMED_ACTION_CHAR;

static struct {
	atomic_t last_anchor_point_in_use;
	uint16_t timed_action_char_handle;
	uint16_t last_anchor_point_event_counter;
	uint64_t last_anchor_point_timestamp;
	struct timed_action timed_action_msg;
	struct bt_gatt_discover_params discovery_params;
} conn_state[CONFIG_BT_MAX_CONN];

static struct {
	uint64_t central_toggle_time_us;
	uint64_t peripheral_toggle_time_us;
	uint8_t conn_index;
} pending_toggle;

static void send_timestamp_to_peripheral(struct bt_conn *conn, void *data)
{
	int err;
	struct bt_conn_info conn_info;
	(void)data;
	uint64_t toggle_time_us = pending_toggle.peripheral_toggle_time_us;

	err = bt_conn_get_info(conn, &conn_info);
	if (err) {
		return;
	}

	uint8_t conn_index = bt_conn_index(conn);

	if (conn_info.state != BT_CONN_STATE_CONNECTED) {
		return;
	}

	if (conn_state[conn_index].timed_action_char_handle == 0) {
		/* Service discovery not yet complete. */
		return;
	}

	if (atomic_test_and_set_bit(&conn_state[conn_index].last_anchor_point_in_use, 0)) {
		return;
	}

	conn_state[conn_index].timed_action_msg.anchor_point_us_central_clock =
		conn_state[conn_index].last_anchor_point_timestamp;
	conn_state[conn_index].timed_action_msg.anchor_point_event_counter =
		conn_state[conn_index].last_anchor_point_event_counter;
	conn_state[conn_index].timed_action_msg.trigger_time_us_central_clock = toggle_time_us;
	conn_state[conn_index].timed_action_msg.led_value = led_value;

	atomic_clear_bit(&conn_state[conn_index].last_anchor_point_in_use, 0);

	//printk("Sending toggle time to peripheral\n");
	//printk("Current time: %lld\n", controller_time_us_get());
	//printk("Scheduled toggle: %lld\n", pending_toggle.peripheral_toggle_time_us);
	//printk("Last anchor point %lld\n", conn_state[pending_toggle.conn_index].last_anchor_point_timestamp);

	err = bt_gatt_write_without_response(conn,
		conn_state[conn_index].timed_action_char_handle,
		&conn_state[conn_index].timed_action_msg,
		sizeof(struct timed_action),
		false);
	if (err) {
		printk("Failed writing to characteristic\n");
	}
}

static void on_timestamp_send_timeout(struct k_work *work)
{
	if (atomic_test_and_set_bit(&conn_state[pending_toggle.conn_index].last_anchor_point_in_use, 0)) {
		return;
	}
	pending_toggle.peripheral_toggle_time_us = conn_state[pending_toggle.conn_index].last_anchor_point_timestamp + 2 * CONN_INTERVAL_US;	
	pending_toggle.central_toggle_time_us = conn_state[pending_toggle.conn_index].last_anchor_point_timestamp + CONN_INTERVAL_US;

	atomic_clear_bit(&conn_state[pending_toggle.conn_index].last_anchor_point_in_use, 0);

	led_value = !led_value;

	timed_led_toggle_trigger_at(led_value, pending_toggle.central_toggle_time_us);

	bt_conn_foreach(BT_CONN_TYPE_LE, send_timestamp_to_peripheral, NULL);
}

K_WORK_DELAYABLE_DEFINE(timestamp_send_work, on_timestamp_send_timeout);

static bool adv_data_parse_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, ADV_NAME_STR_MAX_LEN - 1);
		memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char name_str[ADV_NAME_STR_MAX_LEN] = {0};
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	/* We're only interested in connectable events */
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}

	bt_data_parse(ad, adv_data_parse_cb, name_str);

	if (strncmp(name_str, CONFIG_BT_DEVICE_NAME, ADV_NAME_STR_MAX_LEN) != 0) {
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);

	if (bt_le_scan_stop()) {
		return;
	}

	struct bt_conn *unused_conn = NULL;

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM(CONN_INTERVAL_UNITS, CONN_INTERVAL_UNITS, 0,
						 CONN_TIMEOUT_UNITS), &unused_conn);
	if (err) {
		printk("Create conn to %s failed (%d)\n", addr_str, err);
		scan_start();
	}

	if (unused_conn) {
		bt_conn_unref(unused_conn);
	}
}

static void scan_start(void)
{
	int err;

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE_CONTINUOUS, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning started\n");
}

static uint8_t on_service_discover(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	struct bt_gatt_discover_params *params)
{
	if (attr) {
		uint8_t conn_index = bt_conn_index(conn);

		conn_state[conn_index].timed_action_char_handle =
			bt_gatt_attr_value_handle(attr);
		printk("Service discovery completed\n");
	} else {
		printk("Service discovery failed\n");
	}

	return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s 0x%02x %s\n", addr, err, bt_hci_err_to_str(err));

		scan_start();
		return;
	}

	printk("Connected: %s\n", addr);

	uint8_t conn_index = bt_conn_index(conn);

	conn_state[conn_index].discovery_params.uuid = timed_char_uuid;
	conn_state[conn_index].discovery_params.func = on_service_discover;
	conn_state[conn_index].discovery_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	conn_state[conn_index].discovery_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	conn_state[conn_index].discovery_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

	err = bt_gatt_discover(conn, &conn_state[conn_index].discovery_params);
	if (err) {
		printk("Discovery failed, %d\n", err);
	}

	const uint8_t peripheral_conn_count = 1;

	conn_count++;
	if (conn_count < CONFIG_BT_MAX_CONN - peripheral_conn_count) {
		scan_start();
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	conn_count--;

	uint8_t conn_index = bt_conn_index(conn);

	/* Reset state */
	memset(&conn_state[conn_index], 0, sizeof(conn_state[0]));

	const uint8_t peripheral_conn_count = 1;

	if (conn_count == CONFIG_BT_MAX_CONN - peripheral_conn_count - 1) {
		scan_start();
	}
}

static bool on_conn_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(param);

	return false;
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_req = on_conn_param_req,
};

static bool on_vs_evt(struct net_buf_simple *buf)
{
	uint8_t *subevent_code;
	struct bt_conn *conn;
	sdc_hci_subevent_vs_conn_anchor_point_update_report_t *evt = NULL;

	subevent_code = net_buf_simple_pull_mem(
		buf,
		sizeof(*subevent_code));

	switch (*subevent_code) {
	case SDC_HCI_SUBEVENT_VS_CONN_ANCHOR_POINT_UPDATE_REPORT:
		evt = (void *)buf->data;
		break;
	default:
		return false;
	}

	if (!evt) {
		return false;
	}

	conn = bt_hci_conn_lookup_handle(evt->conn_handle);
	if (!conn) {
		return true;
	}

	uint8_t conn_index = bt_conn_index(conn);

	bt_conn_unref(conn);
	conn = NULL;

	if (atomic_test_and_set_bit(&conn_state[conn_index].last_anchor_point_in_use, 0)) {
		return true;
	}

	conn_state[conn_index].last_anchor_point_timestamp = evt->anchor_point_us;
	conn_state[conn_index].last_anchor_point_event_counter = evt->event_counter;

	atomic_clear_bit(&conn_state[conn_index].last_anchor_point_in_use, 0);

	pending_toggle.conn_index = conn_index;

	k_work_schedule(&timestamp_send_work, K_NO_WAIT);

	return true;
}

void central_start(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	err = bt_hci_register_vnd_evt_cb(on_vs_evt);
	if (err) {
		printk("Failed to register HCI VS callback\n");
		return;
	}

	sdc_hci_cmd_vs_conn_anchor_point_update_event_report_enable_t enable_params = {
		.enable = true
	};

	err = hci_vs_sdc_conn_anchor_point_update_event_report_enable(&enable_params);
	if (err) {
		printk("Failed to enable connection anchor point update events\n");
		return;
	}

	timed_led_toggle_init();

	bt_conn_cb_register(&conn_callbacks);

	scan_start();

	k_work_schedule(&timestamp_send_work, K_MSEC(LED_TOGGLE_PERIOD_MS));
}
