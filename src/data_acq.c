#include "data_acq.h"
#include "uart_handler.h"
#include <zephyr/kernel.h>

/* Semaphore to get a reading. Initial value is 0 and max is 1 */
K_SEM_DEFINE(rg15_ready_sem, 0, 1);

extern const struct device *const my_uart1;

void data_acq_entry(void *a, void *b, void *c) {
    printk("Data thread starting...\n");
    
    k_sem_take(&rg15_ready_sem, K_FOREVER);

    printk("Force rebooting RG-15...\r\n");
	print_uart(my_uart1, "K\n");

    /* Give it a couple of seconds after reboot */
    k_busy_wait(3 * 1000 * 1000);
    print_uart(my_uart1, "P\r\n");
    k_busy_wait(1 * 1000 * 1000);
    
    while(1) {
        printk("Polling measurement from RG-15...\r\n");
        print_uart(my_uart1, "R\r\n");
        k_sleep(K_SECONDS(DEFAULT_DATA_ACQ_PERIODICITY));
    }
}