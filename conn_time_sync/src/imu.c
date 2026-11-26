#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include "conn_time_sync.h"
#include <nrfx_gpiote.h>
#include <gpiote_nrfx.h>
#include <helpers/nrfx_gppi.h>

#define WHO_AM_I_EXP_CONTENT 113
#define WHO_AM_I_REG 15                 // hex: 0x0f
#define CTRL1 16                        // hex: 0x10
#define CTRL3 18                        // hex: 0x12
#define CTRL4 19                        // hex: 0x13
#define CTRL8 23                        // hex: 0x17
#define OUTZ_L_A 40                     // hex: 0x28
#define FIFO_CTRL1 7                    // hex: 0x07
#define FIFO_CTRL2 8                    // hex: 0x08
#define FIFO_CTRL3 9                    // hex: 0x09
#define FIFO_CTRL4 10                   // hex: 0x0a
#define FIFO_STATUS1 27                 // hex: 0x1b
#define FIFO_STATUS2 28                 // hex: 0x1c
#define COUNTER_BDR_REG1 11             // hex: 0x0b
#define COUNTER_BDR_REG2 12             // hex: 0x0c
#define INT1_CTRL 13                    // hex: 0x0d
#define FIFO_DATA_OUT_TAG 120           // hex: 0x78

#define BUF_WORDS 256
#define BYTE_PER_WORD 7
#define BUF_DIM (BUF_WORDS * BYTE_PER_WORD)
#define ODR_HZ 7680
#define REC_SAMPLES (5 * ODR_HZ)
#define MARGIN 10 // Margin to add to REC_TS_DIM
#define REC_TS_DIM (REC_SAMPLES / 128 + MARGIN) // We get 1 timestamp every time the interrupt on INT1 is raised, so one every 128 sample (set in the registres)

/* GPIOTE instance that handles the INT1 pin, and INT1 as absolute pin number
 * (port * 32 + pin) as nrfx wants it. Both are taken from the devicetree.
 */
#define INT1_GPIOTE_NODE NRF_DT_GPIOTE_NODE(DT_NODELABEL(imu), int1_gpios)
#define INT1_PIN NRF_DT_GPIOS_TO_PSEL(DT_NODELABEL(imu), int1_gpios)

static struct k_work_q queue;
static uint64_t rec_duration_s;
static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(imu));
static const struct gpio_dt_spec int1_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(imu), int1_gpios);
static struct gpio_callback int1_cb;
static uint8_t buf[BUF_DIM];
static uint32_t saved_samples;
static uint32_t tot_samples_to_save; 
volatile int16_t rec_buf[REC_SAMPLES];
volatile uint64_t rec_ts[REC_TS_DIM];
volatile uint32_t saved_ts;

K_THREAD_STACK_DEFINE(ds_t_stack, 2048);

static void read_fifo(struct k_work *work) {
    if(saved_samples >= tot_samples_to_save) return;

    if(saved_ts < REC_TS_DIM) {
        rec_ts[saved_ts++] = local_to_central_ns(controller_time_capture_ns_get());
    }

    uint8_t fifo_status[2];
    int err = i2c_burst_read_dt(&i2c_dev, FIFO_STATUS1, fifo_status, sizeof(fifo_status));
    if(err) {
        printk("Error reading the reg: 0x%02x\n", FIFO_STATUS1);
        printk("Error: %d\n", err);
        return;
    }
    uint8_t diff_fifo_mask = 1;   // mask to extract DIFF_FIFO_8
    uint16_t diff_fifo = (uint16_t)((fifo_status[1] & diff_fifo_mask) << 8 | fifo_status[0]);

    uint8_t fifo_overflow_latched_mask = 8; //mask to extract FIFO_OVR_LATCHED
    if(fifo_overflow_latched_mask & fifo_status[1]) {
        printk("FIFO overflow occurred\n");
    }

    if(diff_fifo > BUF_WORDS) {
        diff_fifo = BUF_WORDS;
    }

    if(diff_fifo == 0) {
        return;
    }

    err = i2c_burst_read_dt(&i2c_dev, FIFO_DATA_OUT_TAG, buf, diff_fifo * BYTE_PER_WORD);
    if(err) {
        printk("Error reading the reg: 0x%02x\n", FIFO_DATA_OUT_TAG);
        printk("Error: %d\n", err);
        return;
    }

    uint8_t tag;
    uint8_t acc_nc = 2; // hex: 0x02 -> accelerometer NC (non compressed), new accelerometer data
    uint8_t fifo_data_out1;
    uint8_t fifo_data_out2;
    int16_t acc_z;
    for(int i = 0; i < diff_fifo; ++i) {
        tag = buf[i * BYTE_PER_WORD];
        if((tag >> 3) != acc_nc) {
            continue;
        }
        fifo_data_out1 = buf[i * BYTE_PER_WORD + 1];
        fifo_data_out2 = buf[i * BYTE_PER_WORD + 2];
        acc_z = (int16_t)(fifo_data_out2 << 8 | fifo_data_out1);
        rec_buf[saved_samples++] = acc_z;
        if(saved_samples >= tot_samples_to_save) {
            err = gpio_pin_interrupt_configure_dt(&int1_gpio, GPIO_INT_DISABLE);
            if(err) printk("Failed to disable the interrupt on pin %d. Error: %d\n", int1_gpio.pin, err);
            printk("-------------------------------Recording completed-------------------------------\n");
            printk("Recorded samples: %u\n", saved_samples);
            return;
        }
    }

}

K_WORK_DEFINE(read_fifo_work, read_fifo);

static void int1_cb_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_work_submit_to_queue(&queue, &read_fifo_work);
}

static int set_and_check_reg(uint8_t reg_add, uint8_t val_to_set) {
    uint8_t check_val;
    int err = i2c_reg_write_byte_dt(&i2c_dev, reg_add, val_to_set);
    if(err) {
        printk("Error writing the register: 0x%02x. Error: %d\n", reg_add, err);
        return err;
    }
    err = i2c_reg_read_byte_dt(&i2c_dev, reg_add, &check_val);
    if(err) {
        printk("Error reading the register: 0x%02x. Error: %d\n", reg_add, err);
        return err;
    }
    if(check_val != val_to_set) {
        printk("The value of register: 0x%02x is different from the one set. Set: %d, Read: %d\n", reg_add, val_to_set, check_val);
        return 1;
    }
    return 0;
}

/* Connect the INT1 pin to the RTC0/TIMER0 capture tasks through DPPI, so that
 * every INT1 rising edge latches the controller time in hardware (no CPU, no
 * interrupt latency):
 *
 *   GPIOTE IN event (INT1) --DPPI--> RTC0 CAPTURE[1]
 *                               \--> TIMER0 CAPTURE[1]
 *
 * NOTE: the GPIOTE channel of INT1 is owned by the Zephyr GPIO driver, not by
 * this file. The driver allocates it the first time the edge interrupt is
 * enabled, so the interrupt is enabled here once, the channel is looked up
 * with nrfx and the DPPI connection is attached to its IN event. This works
 * because in NCS 3.4.1 (zephyr/drivers/gpio/gpio_nrfx.c) GPIO_INT_DISABLE only
 * disables the trigger and does not free the channel, and re-enabling reuses
 * the same channel. While the interrupt is disabled the IN event is disabled
 * too, so captures only happen during a recording.
 * Requirement: INT1 must not be in the gpio0 "sense-edge-mask", otherwise the
 * driver uses PORT/SENSE and there is no per-pin IN event.
 *
 * Must be called before CTRL1 starts the accelerometer, so that no INT1 pulse
 * arrives while the connection is being set up.
 */
static int connect_int1_to_capture(void)
{
    nrfx_gpiote_t *gpiote = &GPIOTE_NRFX_INST_BY_NODE(INT1_GPIOTE_NODE);
    nrfx_gppi_handle_t int1_capture_ppi;
    uint8_t ch;

    /* 1. Let the GPIO driver allocate a GPIOTE channel for INT1. */
    int err = gpio_pin_interrupt_configure_dt(&int1_gpio, GPIO_INT_EDGE_RISING);
    if(err) {
        printk("Error enabling the interrupt on pin %d. Error: %d\n", int1_gpio.pin, err);
        return err;
    }

    /* 2. Check that the driver really used a GPIOTE channel (IN event), not SENSE. */
    err = nrfx_gpiote_channel_get(gpiote, INT1_PIN, &ch);
    if(err) {
        printk("INT1 has no GPIOTE channel (sense mode?), cannot capture. Error: %d\n", err);
        return err;
    }

    uint32_t int1_evt = nrfx_gpiote_in_event_address_get(gpiote, INT1_PIN);

    /* 3. INT1 IN event -> RTC0 capture. */
    err = nrfx_gppi_conn_alloc(int1_evt, controller_time_capture_rtc_task_addr_get(), &int1_capture_ppi);
    if(err) {
        printk("Error allocating the DPPI connection INT1 -> RTC capture. Error: %d\n", err);
        return err;
    }

    /* 4. Same connection -> TIMER0 capture, so both are latched at the same instant. */
    err = nrfx_gppi_ep_attach(controller_time_capture_timer_task_addr_get(), int1_capture_ppi);
    if(err) {
        printk("Error attaching TIMER capture to the INT1 DPPI connection. Error: %d\n", err);
        return err;
    }

    /* 5. Activate the connection. */
    nrfx_gppi_conn_enable(int1_capture_ppi);

    /* 6. Disable the interrupt again: dataset_rec() enables it when a recording starts.
     * The channel and the DPPI connection stay in place.
     */
    err = gpio_pin_interrupt_configure_dt(&int1_gpio, GPIO_INT_DISABLE);
    if(err) {
        printk("Error disabling the interrupt on pin %d. Error: %d\n", int1_gpio.pin, err);
        return err;
    }

    printk("INT1 (GPIOTE ch %u) connected to RTC0/TIMER0 capture\n", ch);
    return 0;
}

int imu_init(void)
{
    k_work_queue_init(&queue);
    k_work_queue_start(&queue, ds_t_stack, K_THREAD_STACK_SIZEOF(ds_t_stack), 5, NULL);
    
    if(!device_is_ready(i2c_dev.bus)) {
        printk("I2C bus %s is not ready\n", i2c_dev.bus->name);
        return 1;
    }
  
    int err = i2c_recover_bus(i2c_dev.bus);
    if(err) printk("Error recovering i2c bus\n");

    uint8_t reg_val;
    err = i2c_reg_read_byte_dt(&i2c_dev, WHO_AM_I_REG, &reg_val);
    
    if(err) {
        printk("Error in reading the WHO_AM_I register of the IMU: %d\n", err);
        return err;
    }

    if(reg_val != WHO_AM_I_EXP_CONTENT) {
        printk("Error: the value read from the WHO_AM_I register of the IMU doesn't match the expected one.\n");
        printk("Expected: 0x%02x\n", WHO_AM_I_EXP_CONTENT);
        printk("Read: 0x%02x\n", reg_val);
        return 1;
    }

    if(!gpio_is_ready_dt(&int1_gpio)) {
        printk("GPIO INT1 is not ready\n");
        return 1;
    }

    err = gpio_pin_configure_dt(&int1_gpio, GPIO_INPUT);
    if(err) {
        printk("Error configuring the pin %d for INT1\n", int1_gpio.pin);
        printk("Error: %d\n", err);
        return err;
    }

    gpio_init_callback(&int1_cb, int1_cb_handler, BIT(int1_gpio.pin));
    err = gpio_add_callback_dt(&int1_gpio, &int1_cb);
    if(err) {
        printk("Error adding the callback to the callbacks list\n");
        printk("Error: %d\n", err);
        return err;
    }

    err = connect_int1_to_capture();
    if(err) {
        return err;
    }

    uint8_t ctrl3_val = 68;     // 68 = 01000100b
    err = set_and_check_reg(CTRL3, ctrl3_val);
    if(err) {
        return err;
    }

    uint8_t ctrl4_val = 2;     // 2 = 00000010b
    err = set_and_check_reg(CTRL4, ctrl4_val);
    if(err) {
        return err;
    }

    uint8_t ctrl8_val = 0;   // 0 = ---00000b
    err = set_and_check_reg(CTRL8, ctrl8_val);
    if(err) {
        return err;
    }

    uint8_t fifo_ctrl3_val = 12;    // 12 = 00001100b 
    err = set_and_check_reg(FIFO_CTRL3, fifo_ctrl3_val);
    if(err) {
        return err;
    }

    uint8_t cnt_bdr1_val = 0;   // 0 = 00000000b
    err = set_and_check_reg(COUNTER_BDR_REG1, cnt_bdr1_val);
    if(err) {
        return err;
    }

    uint8_t cnt_bdr2_val = 128;   // 128 = 10000000b
    err = set_and_check_reg(COUNTER_BDR_REG2, cnt_bdr2_val);
    if(err) {
        return err;
    }

    uint8_t int1_ctrl_val = 64;   // 64 = 01000000b
    err = set_and_check_reg(INT1_CTRL, int1_ctrl_val);
    if(err) {
        return err;
    }

    uint8_t fifo_ctrl4_val = 6;   // 6 = 00000110b
    err = set_and_check_reg(FIFO_CTRL4, fifo_ctrl4_val);
    if(err) {
        return err;
    }

    uint8_t ctrl1_val = 12;    // 12 = 00001100b 
    err = set_and_check_reg(CTRL1, ctrl1_val);
    if(err) {
        return err;
    }

    k_msleep(100);

    uint8_t raw[2];
    err = i2c_burst_read_dt(&i2c_dev, OUTZ_L_A, raw, sizeof(raw));
    if(err) {
        printk("Error reading the data of the IMU from 0x%02x. Error: %d\n", OUTZ_L_A, err);
        return err;
    }

    int16_t read_val = (int16_t)(raw[1] << 8 | raw[0]);

    printk("Read test value from IMU: %d\n", read_val);
    printk("IMU successfully initialized!\n");
    return 0;
}

static void dataset_rec(uint64_t duration) 
{
    saved_samples = 0;
    saved_ts = 0;
    uint64_t samples_to_save = duration * ODR_HZ;
    if(samples_to_save > REC_SAMPLES) {
        printk("The duration of the recording is too long\n");
        printk("The recording will be truncated to %d samples\n", REC_SAMPLES);
        tot_samples_to_save = REC_SAMPLES;
    }
    else {
        tot_samples_to_save = samples_to_save;
    }

    int err = set_and_check_reg(FIFO_CTRL4, 0); // bypass mode: delete FIFO content (data read before recording starts)
    if(err) {
        return;
    }

    err = set_and_check_reg(FIFO_CTRL4, 6); // FIFO continuous mode: it starts again empty
    if(err) {
        return;
    }

    printk("-------------------------------Starting the recording-------------------------------\n");
    err = gpio_pin_interrupt_configure_dt(&int1_gpio, GPIO_INT_EDGE_RISING);
    if(err) {
        printk("Error in configuring the interrupt on pin: %d\n", int1_gpio.pin);
        printk("Error: %d\n", err);
        return;
    }

    return;
}

static void dataset_rec_work(struct k_work *work)
{
    (void)work;
	dataset_rec(rec_duration_s);
}
K_WORK_DELAYABLE_DEFINE(start_dataset_recording, dataset_rec_work);

void dataset_rec_schedule(k_timeout_t delay, uint64_t duration_s) 
{
    int err;
    rec_duration_s = duration_s;
    err = k_work_schedule_for_queue(&queue, &start_dataset_recording, delay);
    if(err < 0) {
        printk("Error scheduling the work for dataset recording\n");
    }
}