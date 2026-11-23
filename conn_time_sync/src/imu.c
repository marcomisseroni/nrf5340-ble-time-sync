#include <zephyr/drivers/i2c.h>
#include "conn_time_sync.h"

#define WHO_AM_I_REG 15
#define WHO_AM_I_EXP_CONTENT 113

static struct k_work_q queue;
static uint64_t rec_duration_s;
static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(imu));

K_THREAD_STACK_DEFINE(ds_t_stack, 2048);

void imu_init(void) 
{
    k_work_queue_init(&queue);
    k_work_queue_start(&queue, ds_t_stack, K_THREAD_STACK_SIZEOF(ds_t_stack), 5, NULL);

    if(!device_is_ready(i2c_dev.bus)) {
        printk("I2C bus %s is not ready\n", i2c_dev.bus->name);
        return;
    }

    uint8_t reg_val;
    int err = i2c_reg_read_byte_dt(&i2c_dev, WHO_AM_I_REG, &reg_val);
    
    if(err) {
        printk("Error in reading the WHO_AM_I register of the IMU: %d\n", err);
        return;
    }

    if(reg_val != WHO_AM_I_EXP_CONTENT) {
        printk("Error: the value read from the WHO_AM_I register of the IMU doesn't match the expected one.\n");
        printk("Expected: %d\n", WHO_AM_I_EXP_CONTENT);
        printk("Read: 0x%02x\n", reg_val);
        return;
    }

    printk("IMU successfully initialized!\n");
}

static void dataset_rec(uint64_t rec_duration) 
{
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