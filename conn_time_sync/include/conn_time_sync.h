/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef CONN_TIME_SYNC_H__
#define CONN_TIME_SYNC_H__

/**
 * @file
 * @defgroup conn_time_sync Definitions for the connection time sync sample.
 * @{
 * @brief Definitions for the connection time sync sample.
 *
 * This file contains common definitions and API declarations
 * used by the sample.
 */

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/** @brief  Timed action service UUID */
#define BT_UUID_TIMED_ACTION_SERVICE_VAL \
	BT_UUID_128_ENCODE(0xA88278D0, 0x7009, 0x4BEE, 0xA6F8, 0xE1DC3FF02B92)
#define BT_UUID_TIMED_ACTION_SERVICE \
	BT_UUID_DECLARE_128(BT_UUID_TIMED_ACTION_SERVICE_VAL)

/** @brief Timed action characteristic UUID */
#define BT_UUID_TIMED_ACTION_CHAR_VAL \
	BT_UUID_128_ENCODE(0xA88278D1, 0x7009, 0x4BEE, 0xA6F8, 0xE1DC3FF02B92)
#define BT_UUID_TIMED_ACTION_CHAR \
	BT_UUID_DECLARE_128(BT_UUID_TIMED_ACTION_CHAR_VAL)

/** @brief Start central demo. */
void central_start(void);

/** @brief Start peripheral demo. */
void peripheral_start(void);

/** @brief The timed action message
 *
 * The timed action message represents the information
 * sent from the central to the peripheral to allow time synchronization.
 * The anchor point information is the common time reference between
 * the central and peripheral.
 *
 * This samples transmits the anchor point time and trigger time
 * in a single packet. Other applications may consider sending those
 * separately.
 */
struct timed_action {
	uint64_t anchor_point_us_central_clock;
	uint16_t anchor_point_event_counter;
	uint64_t trigger_time_us_central_clock;
	uint64_t start_rec_time_us_central_clock;
	uint64_t rec_duration_s;
	bool start_rec_time_available;
	bool led_value;
} __packed;

static inline void timed_action_print(struct timed_action *action)
{
	printk("c_anchor point (time=%llu, counter=%u), c_trigger_time %llu, value %d\n",
		action->anchor_point_us_central_clock,
		action->anchor_point_event_counter,
		action->trigger_time_us_central_clock,
		action->led_value);
}

/** @brief Obtain the current Bluetooth controller time.
 *
 * The timestamps are based upon this clock.
 *
 * @return The current controller time.
 */
uint64_t controller_time_us_get(void);

/** @brief Set the controller to trigger a PPI event at the given timestamp.
 *
 * @param timestamp_us The timestamp where it will trigger.
 */
void controller_time_trigger_set(uint64_t timestamp_us);

/** @brief Set a constant offset applied to every trigger.
 *
 * @param offset_ns The offset in nanoseconds. Positive values delay the trigger.
 */
void controller_time_trigger_offset_ns_set(int32_t offset_ns);

/** @brief Get the address of the event that will trigger.
 *
 * @return The address of the event that will trigger.
 */
uint32_t controller_time_trigger_event_addr_get(void);

/** @brief Get the address of the task that captures the RTC0 counter.
 *
 * Connect an event to this task (e.g. with nrfx_gppi) to latch the RTC0
 * counter into capture channel 1 at the moment the event happens.
 * Use together with controller_time_capture_timer_task_addr_get() on the
 * same event, so that RTC0 and TIMER0 are captured at the same instant.
 *
 * Only implemented for the nRF5340 application core (controller_time_nrf53_app.c).
 *
 * @return The address of the RTC0 CAPTURE[1] task.
 */
uint32_t controller_time_capture_rtc_task_addr_get(void);

/** @brief Get the address of the task that captures the TIMER0 counter.
 *
 * TIMER0 runs at 16 MHz and is cleared on every RTC0 tick, so the captured
 * value is the sub-tick part of the timestamp (0 to about 488 counts).
 *
 * Only implemented for the nRF5340 application core (controller_time_nrf53_app.c).
 *
 * @return The address of the TIMER0 CAPTURE[1] task.
 */
uint32_t controller_time_capture_timer_task_addr_get(void);

/** @brief Read the raw values latched by the last capture.
 *
 * Returns the values as they are in the capture registers, without
 * conversion to time and without the RTC overflow count. A new capture
 * overwrites them, so read them before the next event.
 *
 * Only implemented for the nRF5340 application core (controller_time_nrf53_app.c).
 *
 * @param rtc_ticks    Output: RTC0 counter (32768 Hz ticks, 24 bit).
 * @param timer_counts Output: TIMER0 counter (16 MHz counts since the last RTC0 tick).
 */
void controller_time_capture_get(uint32_t *rtc_ticks, uint32_t *timer_counts);

/** @brief Get the controller time of the last capture, in nanoseconds.
 *
 * Combines the RTC0 and TIMER0 values latched by the last capture event
 * (e.g. an IMU INT1 pulse) with the RTC overflow count, so the result is the
 * full controller time at which the event happened, with sub-µs resolution
 * (TIMER0 step: 62.5 ns). This is not the current time: for that use
 * controller_time_us_get().
 *
 * If the RTC overflowed between the capture and this call, the capture is
 * assigned to the previous overflow period. Call it within a few ms of the
 * event (well before the next capture and much less than the 512 s RTC period).
 *
 * Only implemented for the nRF5340 application core (controller_time_nrf53_app.c).
 *
 * @return Controller time of the last capture, in ns.
 */
uint64_t controller_time_capture_ns_get(void);

/** @brief Initialize the module handling timed toggling of an LED.
 *
 * @return 0 on success, failure otherwise.
 */
int timed_led_toggle_init(void);

/** @brief Toggle the LED using the given value at the given timestamp.
 *
 * @param value The LED value.
 * @param timestamp_us The time when the LED will be set, in microseconds.
 *                     The time is specified in controller clock units.
 */
void timed_led_toggle_trigger_at(uint8_t value, uint32_t timestamp_us);

/** @brief Initialize the IMU (LSM6DSV16BXTR) and the dataset recording work queue.
 *
 * Starts the work queue that runs the dataset recording, checks that the I2C
 * bus is ready and that WHO_AM_I matches, then configures the accelerometer
 * (CTRL3: BDU and IF_INC, CTRL8: full scale, CTRL1: ODR and operating mode,
 * written last because it powers the accelerometer on). Each written register
 * is read back to verify its content.
 *
 * Must be called before dataset_rec_schedule(), in both roles.
 *
 * @return 0 on success.
 * @return Negative error code from the I2C API if a register access fails.
 * @return 1 if the I2C bus is not ready, WHO_AM_I does not match, or a
 *         register read back does not match the written value.
 */
int imu_init(void);

/** @brief Schedule the recording of the LSM6DSV16BXTR data into RAM.
 *
 * The recording runs on the dedicated work queue started by imu_init(),
 * so it does not block the system work queue.
 *
 * @param delay Delay before the recording starts.
 * @param duration_s Duration of the recording, in seconds.
 */
void dataset_rec_schedule(k_timeout_t delay, uint64_t duration_s);

/** @brief Convert a local controller time to the central controller time.
 *
 * On the peripheral, subtracts the offset (local - central) computed at the
 * last connection anchor point from the last timed_action received. On the
 * central the offset stays 0, so the time is returned unchanged.
 *
 * @param local_ns Local controller time, in ns.
 * @return The same instant in the central controller time, in ns.
 */
uint64_t local_to_central_ns(uint64_t local_ns);

#endif

/**
 * @}
 */
