#include "data_acq.h"
#include <zephyr/kernel.h>

/* Semaphore to get a reading. Initial value is 0 and max is 1 */
K_SEM_DEFINE(get_rg15_reading_sem, 0, 1);

void data_acq_entry(void *a, void *b, void *c) {
    
    while(1) {
        k_sem_give(&get_rg15_reading_sem);
        k_sleep(K_SECONDS(DEFAULT_DATA_ACQ_PERIODICITY));
    }
}

void rg15_acq_thread(void *a, void *b, void *c) {

    while(1) {
        if (k_sem_take(&get_rg15_reading_sem, K_FOREVER) == 0) {
            /* Do RG-15 data acq here */
        }
    }
}