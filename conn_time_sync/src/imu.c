#include "conn_time_sync.h"

K_THREAD_STACK_DEFINE(ds_t_stack, 2048);

static struct k_work_q queue;
static uint64_t rec_duration_s;

void imu_init(void) 
{
    k_work_queue_init(&queue);
    k_work_queue_start(&queue, ds_t_stack, K_THREAD_STACK_SIZEOF(ds_t_stack), 5, NULL);
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