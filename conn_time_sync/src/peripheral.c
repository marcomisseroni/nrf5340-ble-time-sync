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
#include <bluetooth/services/nus_client.h>
#include <bluetooth/hci_vs_sdc.h>

#include "conn_time_sync.h"

#define PERIPHERAL_TRIGGER_CORRECTION_NS 0

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static atomic_t last_timed_action_in_use;
static struct timed_action last_timed_action;
static bool dataset_rec_set = false;
static int64_t offset_us;
static struct k_spinlock offset_lock;

static uint32_t conn_interval_us_get(const struct bt_conn *conn)
{
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info)) {
		return 0;
	}

	return info.le.interval_us;
}

static ssize_t time_sync_data_received(struct bt_conn *conn,
					     const struct bt_gatt_attr *attr,
					     const void *buf, uint16_t len,
					     uint16_t offset, uint8_t flags)
{
	__ASSERT_NO_MSG(len == sizeof(struct timed_action));
	__ASSERT_NO_MSG(offset == 0);

	struct timed_action *params = (void *)buf;

	__ASSERT_NO_MSG(params->trigger_time_us_central_clock >
			params->anchor_point_us_central_clock);

	if (!atomic_test_and_set_bit(&last_timed_action_in_use, 0)) {
		memcpy(&last_timed_action, params, sizeof(*params));
		atomic_clear_bit(&last_timed_action_in_use, 0);
	}

	//printk("Received: ");

	//timed_action_print(params);

	return len;
}

BT_GATT_SERVICE_DEFINE(timed_toggle_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_TIMED_ACTION_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TIMED_ACTION_CHAR,
		BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, time_sync_data_received, NULL));

static void adv_start(void)
{
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising started\n");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s 0x%02x %s\n", addr, err, bt_hci_err_to_str(err));

		adv_start();
		return;
	}

	printk("Connected: %s\n", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	adv_start();
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static uint64_t central_timestamp_to_local_clock(uint64_t central_timestamp_us,
						 uint32_t conn_interval_us,
						 uint64_t central_anchor_us,
						 uint64_t peripheral_anchor_us,
						 uint16_t central_event_count,
						 uint16_t peripheral_event_count)
{
	int32_t event_counter_diff = (int16_t)(central_event_count - peripheral_event_count);
	int32_t anchor_point_time_diff_us = event_counter_diff * conn_interval_us;
	uint64_t peripheral_time_us_at_central_anchor =
		peripheral_anchor_us + anchor_point_time_diff_us;
	int64_t anchor_to_timestamp = central_timestamp_us - central_anchor_us;

	return peripheral_time_us_at_central_anchor + anchor_to_timestamp;
}

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

	uint32_t conn_interval_us = conn_interval_us_get(conn);

	bt_conn_unref(conn);
	conn = NULL;

	if (atomic_test_and_set_bit(&last_timed_action_in_use, 0)) {
		return true;
	}

	uint64_t controller_time_us = controller_time_us_get();

	uint64_t local_at_central_anchor = central_timestamp_to_local_clock(
		last_timed_action.anchor_point_us_central_clock,
		conn_interval_us,
		last_timed_action.anchor_point_us_central_clock,
		evt->anchor_point_us,
		last_timed_action.anchor_point_event_counter,
		evt->event_counter);

	k_spinlock_key_t key = k_spin_lock(&offset_lock);
	offset_us = (int64_t)local_at_central_anchor - (int64_t)last_timed_action.anchor_point_us_central_clock;
	k_spin_unlock(&offset_lock, key);

	uint64_t trigger_time_local = central_timestamp_to_local_clock(
		last_timed_action.trigger_time_us_central_clock,
		conn_interval_us,
		last_timed_action.anchor_point_us_central_clock,
		evt->anchor_point_us,
		last_timed_action.anchor_point_event_counter,
		evt->event_counter);

	if (controller_time_us + 500 < trigger_time_local) {
		timed_led_toggle_trigger_at(last_timed_action.led_value, trigger_time_local);
	}
	
	if(last_timed_action.start_rec_time_available && !dataset_rec_set) {
		uint64_t begin_dataset_rec_time = central_timestamp_to_local_clock(
			last_timed_action.start_rec_time_us_central_clock,
			conn_interval_us,
			last_timed_action.anchor_point_us_central_clock,
			evt->anchor_point_us,
			last_timed_action.anchor_point_event_counter,
			evt->event_counter);
		if(begin_dataset_rec_time < controller_time_us_get()) {
			printk("Error: dataset recording should have started in the past\n");
		}
		else {
			dataset_rec_schedule(K_MSEC((begin_dataset_rec_time - controller_time_us_get()) / 1000), last_timed_action.rec_duration_s);
			dataset_rec_set = true;
		}
	}

	atomic_clear_bit(&last_timed_action_in_use, 0);

	return true;
}

void peripheral_start(void)
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

	/* Correction for the systematic error of the peripheral trigger. */
	controller_time_trigger_offset_ns_set(PERIPHERAL_TRIGGER_CORRECTION_NS);

	timed_led_toggle_init();

	bt_conn_cb_register(&conn_callbacks);

	adv_start();
}

uint64_t local_to_central_ns(uint64_t local_ns)
{
	k_spinlock_key_t key = k_spin_lock(&offset_lock);
	int64_t off = offset_us;
	k_spin_unlock(&offset_lock, key);

	return local_ns - off * 1000;
}