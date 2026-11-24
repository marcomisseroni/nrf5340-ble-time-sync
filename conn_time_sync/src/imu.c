#include <zephyr/drivers/i2c.h>
#include "conn_time_sync.h"

#define WHO_AM_I_EXP_CONTENT 113
#define WHO_AM_I_REG 15                 // hex: 0x0f
#define CTRL1 16                        // hex: 0x10
#define CTRL3 18                        // hex: 0x12
#define CTRL8 23                        // hex: 0x17
#define OUTZ_L_A 40                     // hex: 0x28

static struct k_work_q queue;
static uint64_t rec_duration_s;
static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(imu));

K_THREAD_STACK_DEFINE(ds_t_stack, 2048);

int imu_init(void) 
{
    k_work_queue_init(&queue);
    k_work_queue_start(&queue, ds_t_stack, K_THREAD_STACK_SIZEOF(ds_t_stack), 5, NULL);

    if(!device_is_ready(i2c_dev.bus)) {
        printk("I2C bus %s is not ready\n", i2c_dev.bus->name);
        return 1;
    }

    uint8_t reg_val;
    int err = i2c_reg_read_byte_dt(&i2c_dev, WHO_AM_I_REG, &reg_val);
    
    if(err) {
        printk("Error in reading the WHO_AM_I register of the IMU: %d\n", err);
        return err;
    }

    if(reg_val != WHO_AM_I_EXP_CONTENT) {
        printk("Error: the value read from the WHO_AM_I register of the IMU doesn't match the expected one.\n");
        printk("Expected: %d\n", WHO_AM_I_EXP_CONTENT);
        printk("Read: 0x%02x\n", reg_val);
        return 1;
    }

    bool successful_init = true;
    uint8_t check_val;
    uint8_t ctrl3_val = 68;     // 68 = 01000100b
    err = i2c_reg_write_byte_dt(&i2c_dev, CTRL3, ctrl3_val);
    if(err) {
        printk("Error writing the register CTRL3: 0x%02x. Error: %d\n", CTRL3, err);
        return err;
    }
    err = i2c_reg_read_byte_dt(&i2c_dev, CTRL3, &check_val);
    if(err) {
        printk("Error reading the register CTRL3: 0x%02x. Error: %d\n", CTRL3, err);
        return err;
    }
    if(check_val != ctrl3_val) {
        printk("The value of register CTRL3: 0x%02x is different from the one set. Set: %d, Read: %d\n", CTRL3, ctrl3_val, check_val);
        successful_init = false;
    }

    uint8_t ctrl8_val = 0;   // 0 = ---00000b
    err = i2c_reg_write_byte_dt(&i2c_dev, CTRL8, ctrl8_val);
    if(err) {
        printk("Error writing the register CTRL8: 0x%02x. Error: %d\n", CTRL8, err);
        return err;
    }
    err = i2c_reg_read_byte_dt(&i2c_dev, CTRL8, &check_val);
    if(err) {
        printk("Error reading the register CTRL8: 0x%02x. Error: %d\n", CTRL8, err);
        return err;
    }
    if(check_val != ctrl8_val) {
        printk("The value of register CTRL8: 0x%02x is different from the one set. Set: %d, Read: %d\n", CTRL8, ctrl8_val, check_val);
        successful_init = false;
    }

    uint8_t ctrl1_val = 12;    // 12 = 00001100b 
    err = i2c_reg_write_byte_dt(&i2c_dev, CTRL1, ctrl1_val);
    if(err) {
        printk("Error writing the register CTRL1: 0x%02x. Error: %d\n", CTRL1, err);
        return err;
    }
    err = i2c_reg_read_byte_dt(&i2c_dev, CTRL1, &check_val);
    if(err) {
        printk("Error reading the register CTRL1: 0x%02x. Error: %d\n", CTRL1, err);
        return err;
    }
    if(check_val != ctrl1_val) {
        printk("The value of register CTRL1: 0x%02x is different from the one set. Set: %d, Read: %d\n", CTRL1, ctrl1_val, check_val);
        successful_init = false;
    }

    if(successful_init) {
        printk("IMU successfully initialized!\n");
    }
    return !successful_init;
}

static void dataset_rec(uint64_t rec_duration) 
{
    uint8_t raw[2];
    int err = i2c_burst_read_dt(&i2c_dev, OUTZ_L_A, raw, sizeof(raw));
    if(err) {
        printk("Error reading the data of the IMU from 0x%02x. Error: %d\n", OUTZ_L_A, err);
        return;
    }

    int16_t read_val = (int16_t)(raw[1] << 8 | raw[0]);

    printk("Read test value from IMU: %d\n", read_val);

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