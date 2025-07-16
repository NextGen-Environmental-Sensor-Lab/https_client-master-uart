#include <helpers/nrfx_reset_reason.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "battery.h"
#include "data_acq.h"
#include "https_handler.h"
#include "uart_handler.h"

#define UART_THREAD_STACK_SIZE     2 * 1024
#define CONSOLE_THREAD_STACK_SIZE  10 * 1024
#define HTTPS_THREAD_STACK_SIZE    10 * 1024
#define DATA_ACQ_THREAD_STACK_SIZE 10 * 1024

K_THREAD_STACK_DEFINE(uart_thread_stack, UART_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(https_thread_stack, HTTPS_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(data_acq_thread_stack, DATA_ACQ_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(console_thread_stack, CONSOLE_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(watchdog_thread_area, 4096);

static struct k_thread uart_thread;
static struct k_thread https_thread;
static struct k_thread data_acq_thread;
static struct k_thread watchdog_thread_data;
static struct k_thread console_thread;

/* Stores last reset reason string */
char reset_reason_str[128];

extern void uart_thread_entry(void *, void *, void *);
extern void https_thread_entry(void *, void *, void *);
extern void data_acq_entry(void *, void *, void *);
extern void watchdog_feeder_thread(void *, void *, void *);
extern void console_thread_entry(void *, void *, void *);

extern const struct device *const my_uart1;

static void reset_reason_str_get(char *str, uint32_t reason) {
        size_t len;

        *str = '\0';

        if (reason & NRFX_RESET_REASON_RESETPIN_MASK) {
                (void)strcat(str, "PIN reset | ");
        }
        if (reason & NRFX_RESET_REASON_DOG_MASK) {
                (void)strcat(str, "watchdog | ");
        }
        if (reason & NRFX_RESET_REASON_OFF_MASK) {
                (void)strcat(str, "wakeup from power-off | ");
        }
        if (reason & NRFX_RESET_REASON_DIF_MASK) {
                (void)strcat(str, "debug interface wakeup | ");
        }
        if (reason & NRFX_RESET_REASON_SREQ_MASK) {
                (void)strcat(str, "software | ");
        }
        if (reason & NRFX_RESET_REASON_LOCKUP_MASK) {
                (void)strcat(str, "CPU lockup | ");
        }
        if (reason & NRFX_RESET_REASON_CTRLAP_MASK) {
                (void)strcat(str, "control access port | ");
        }

        len = strlen(str);
        if (len == 0) {
                (void)strcpy(str, "power-on reset");
        } else {
                str[len - 3] = '\0';
        }
}

static void print_reset_reason(void) {
        uint32_t reset_reason;

        /* Read RESETREAS register value and clear current reset reason(s). */
        reset_reason = nrfx_reset_reason_get();

        /* Reset reason can only be read once */
        nrfx_reset_reason_clear(reset_reason);

        reset_reason_str_get(reset_reason_str, reset_reason);

        printk("\nReset reason: %s\n", reset_reason_str);
}

int main(void) {
        int ret;

        /* Also loads the reset reason string into the buffer */
        print_reset_reason();

        /* Initialize the RG-15 UART */
        ret = uart_init(my_uart1);
        if (ret != 0) {
                printk("uart_init FATAL ERROR!\n");
                return -1;
        }

        /* Initialize HTTPS Client */
        ret = https_init();
        if (ret != 0) {
                printk("https_init FATAL ERROR\n");
                return -1;
        }

        /* Initialize battery measurement */
        ret = init_adc();
        if (ret == 0) {
                printk("battery_init FATAL ERROR\n");
                return -1;
        }

        /* Initialize data acquisition */
        ret = data_acq_init();
        if (ret != 0) {
                printk("data_acq_init FATAL ERROR\n");
                return -1;
        }

        /* Start UART thread */
        k_thread_create(&uart_thread,
                        uart_thread_stack,
                        K_THREAD_STACK_SIZEOF(uart_thread_stack),
                        uart_thread_entry,
                        NULL,
                        NULL,
                        NULL,
                        6,
                        0,
                        K_NO_WAIT);

	/* Start console thread */
        k_thread_create(&console_thread,
                        console_thread_stack,
                        K_THREAD_STACK_SIZEOF(console_thread_stack),
                        console_thread_entry,
                        NULL,
                        NULL,
                        NULL,
                        9,
                        0,
                        K_NO_WAIT);

        /* Start HTTPS thread */
        k_thread_create(&https_thread,
                        https_thread_stack,
                        K_THREAD_STACK_SIZEOF(https_thread_stack),
                        https_thread_entry,
                        NULL,
                        NULL,
                        NULL,
                        7,
                        0,
                        K_NO_WAIT);

        /* Start Data acq thread */
        k_thread_create(&data_acq_thread,
                        data_acq_thread_stack,
                        K_THREAD_STACK_SIZEOF(data_acq_thread_stack),
                        data_acq_entry,
                        NULL,
                        NULL,
                        NULL,
                        5,
                        0,
                        K_NO_WAIT);

        k_thread_create(&watchdog_thread_data,
                        watchdog_thread_area,
                        K_THREAD_STACK_SIZEOF(watchdog_thread_area),
                        watchdog_feeder_thread,
                        NULL,
                        NULL,
                        NULL,
                        8,
                        0,
                        K_NO_WAIT);
        while (1) {
                k_sleep(K_FOREVER);
        }
}
