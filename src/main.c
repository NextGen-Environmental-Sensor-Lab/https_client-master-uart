#include <string.h>
#include <zephyr/kernel.h>
#include <stdlib.h>

#include "uart_handler.h"
#include "https_handler.h"

#define UART_THREAD_STACK_SIZE 	2 * 1024
#define HTTPS_THREAD_STACK_SIZE 2 * 1024
#define DATA_ACQ_THREAD_STACK_SIZE 2 * 1024

K_THREAD_STACK_DEFINE(uart_thread_stack, UART_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(https_thread_stack, HTTPS_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(data_acq_thread_stack, DATA_ACQ_THREAD_STACK_SIZE);

static struct k_thread uart_thread;
static struct k_thread https_thread;
static struct k_thread data_acq_thread;

extern void uart_thread_entry(void *, void *, void *);
extern void https_thread_entry(void *, void *, void *);
extern void data_acq_entry(void *, void *, void *);

extern const struct device *const my_uart0;
extern const struct device *const my_uart1;

int main(void) {
	int ret;

	// /* Initialize the UART console */
	// ret = uart_init(my_uart0);
	// if (ret != 0) {
	// 	printk("FATAL ERROR!\n");
	// 	return -1;
	// }

	/* Initialize the Sensor's UART */
	ret = uart_init(my_uart1);
	if (ret != 0) {
		printk("FATAL ERROR!\n");
		return -1;
	}

	/* Initialize HTTPS Client */
	ret = https_init();
	if (ret != 0) {
		printk("FATAL ERROR\n");
		return -1;
	}

	/* Start UART thread */
	k_thread_create(&uart_thread, uart_thread_stack,
		K_THREAD_STACK_SIZEOF(uart_thread_stack), uart_thread_entry,
		NULL, NULL, NULL, 6, 0, K_NO_WAIT);

	/* Start HTTPS thread */
	k_thread_create(&https_thread, https_thread_stack,
		K_THREAD_STACK_SIZEOF(https_thread_stack), https_thread_entry,
		NULL, NULL, NULL, 7, 0, K_NO_WAIT);

	/* Start Data acq thread */
	k_thread_create(&data_acq_thread, data_acq_thread_stack,
		K_THREAD_STACK_SIZEOF(data_acq_thread_stack), data_acq_entry,
		NULL, NULL, NULL, 5, 0, K_NO_WAIT);

	while (1) {
		k_sleep(K_FOREVER);
	}
}