#include "console.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

K_MSGQ_DEFINE(console_msgq, CONSOLE_MSG_SIZE, 10, 4);

static const struct device *const console_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

typedef struct {
        const char *name;
        void (*function)(char *buf, char *response);
} command_t;

static int console_rx_buf_pos;
static char console_rx_buf[CONSOLE_MSG_SIZE];

static void command_sensor_ok(char *buf, char *response);
static void command_sensor_reboot(char *buf, char *response);

command_t const sensor_console_commands[] = {
    {"CMD_SENSOR_OK",     command_sensor_ok    },
    {"CMD_SENSOR_REBOOT", command_sensor_reboot},
    {NULL,                NULL                 }
};

void console_rx_cb(const struct device *dev, void *user_buf) {
        uint8_t c;

        if (!uart_irq_update(console_dev)) {
                return;
        }

        if (!uart_irq_rx_ready(console_dev)) {
                return;
        }

        /* read until FIFO empty */
        while (uart_fifo_read(console_dev, &c, 1) == 1) {
                if ((c == '\n' || c == '\r') && console_rx_buf_pos > 0) {
                        /* terminate string */
                        console_rx_buf[console_rx_buf_pos] = '\0';

                        /* if queue is full, message is silently dropped */
                        k_msgq_put(&console_msgq, &console_rx_buf, K_NO_WAIT);

                        /* reset the buffer (it was copied to the msgq) */
                        console_rx_buf_pos = 0;
                } else if (console_rx_buf_pos < (sizeof(console_rx_buf) - 1)) {
                        console_rx_buf[console_rx_buf_pos++] = c;
                }
                /* else: characters beyond buffer size are dropped */
        }
}

static void exec_console_command(const char *buf) {
        if (buf == NULL) {
                return;
        }
        if ((strlen(buf) == 1) && (*buf == '\n' || *buf == '\r')) {
                return;
        }

        char cmd[CONSOLE_MSG_SIZE];
        char cmd_response[CONSOLE_MSG_SIZE] = {0};
        int idx                             = 0;
        int command_found                   = 0;

        while (*buf != '\0') {
                if (*buf != '\n') {
                        cmd[idx] = *buf;
                        idx++;
                }
                buf++;
        }
        cmd[idx] = '\0';

        for (int i = 0; sensor_console_commands[i].name != NULL; i++) {
                if (strstr(cmd, sensor_console_commands[i].name) != NULL) {
                        /* command matched */
                        (*sensor_console_commands[i].function)(cmd, cmd_response);
                        command_found = 1;
                        break;
                }
        }
        if (!command_found) {
                /* inavlid command */
                printk("Invalid sensor command!\n");
                snprintf(cmd_response, CONSOLE_MSG_SIZE, "%s", "Invalid sensor command!\n");
                return;
        }
        printk("%s\n", cmd_response);
}

void console_queue_command(const char *buf) {
        k_msgq_put(&console_msgq, buf, K_NO_WAIT);
}

static void command_sensor_ok(char *buf, char *response) {
        printk("Sensor is OK\n");
}

static void command_sensor_reboot(char *buf, char *response) {
        sys_reboot(SYS_REBOOT_COLD);
}

void console_thread_entry(void *a, void *b, void *c) {
        char buf[CONSOLE_MSG_SIZE];

        if (!device_is_ready(console_dev)) {
                printk("UART device not found!");
                return;
        }

        /* configure interrupt and callback to receive buf */
        int ret = uart_irq_callback_user_data_set(console_dev, console_rx_cb, NULL);

        if (ret < 0) {
                if (ret == -ENOTSUP) {
                        printk("Interrupt-driven UART API support not enabled\n");
                } else if (ret == -ENOSYS) {
                        printk("UART device does not support interrupt-driven API\n");
                } else {
                        printk("Error setting UART callback: %d\n", ret);
                }
                return;
        }
        uart_irq_rx_enable(console_dev);

        printk("Console thread started!\n");

        while (1) {

                /* indefinitely wait for input from the user */
                while (k_msgq_get(&console_msgq, &buf, K_FOREVER) == 0) {
                        exec_console_command(buf);
                }
        }
}
