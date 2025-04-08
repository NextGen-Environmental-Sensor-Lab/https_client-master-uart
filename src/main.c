#include <string.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "uart_handler.h"
#include "https_handler.h"
#include "battery.h"

#define UART_THREAD_STACK_SIZE 	2 * 1024
#define HTTPS_THREAD_STACK_SIZE 2 * 1024
#define DATA_ACQ_THREAD_STACK_SIZE 2 * 1024

K_THREAD_STACK_DEFINE(uart_thread_stack, UART_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(https_thread_stack, HTTPS_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(data_acq_thread_stack, DATA_ACQ_THREAD_STACK_SIZE);

static struct k_thread uart_thread;
static struct k_thread https_thread;
static struct k_thread data_acq_thread;

//static const struct device *gpio_port = DEVICE_DT_GET(DT_NODELABEL(gpio0));

extern void uart_thread_entry(void *, void *, void *);
extern void https_thread_entry(void *, void *, void *);
extern void data_acq_entry(void *, void *, void *);

extern const struct device *const my_uart1;

extern int battery_measure_enable(bool enable);
extern int battery_sample(void);

int main(void) {
	int ret;

	/* Initialize the RG-15 UART */
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

	int rc = battery_measure_enable(true);
	if (rc != 0) {
		printk("Failed initialize battery measurement: %d\n", rc);
		return 0;
	}

	int batt_mV = battery_sample();
	if (batt_mV < 0) {
		printk("Failed to read battery voltage: %d\n",
				batt_mV);
	
	}

	printk("%d mV\n", batt_mV);

	k_busy_wait(1 * USEC_PER_SEC);

	printk("Disable: %d\n", battery_measure_enable(false));

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