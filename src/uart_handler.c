#include "uart_handler.h"
#include "https_handler.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

#define DEV_CONSOLE DT_NODELABEL(uart0)
#define DEV_OTHER DT_NODELABEL(uart1)

#define MSG_SIZE 32

extern struct k_sem get_reading_sem;

static char uart_send[SEND_BUF_SIZE];

extern struct k_msgq https_send_queue;

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart0_msgq, MSG_SIZE, 10, 4);
K_MSGQ_DEFINE(uart1_msgq, MSG_SIZE, 10, 4);

static const struct device *const my_uart0 = DEVICE_DT_GET(DEV_CONSOLE);
static const struct device *const my_uart1 = DEVICE_DT_GET(DEV_OTHER);

/* receive buffer used in UART ISR callback */
static char tx_buf[MSG_SIZE];
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void uart0_cb(const struct device *dev, void *user_data) {
	uint8_t c;
	
	if (!uart_irq_update(my_uart0)) {	
		return;
	}
	if (!uart_irq_rx_ready(my_uart0)) {	
		return;
	}
	/* read until FIFO empty */
	while (uart_fifo_read(my_uart0, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';
			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart0_msgq, &rx_buf, K_NO_WAIT);
			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		}
		else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
	}
}

void uart1_cb(const struct device *dev, void *user_data) {
	uint8_t c;
	if (!uart_irq_update(my_uart1)) {
		return;
	}
	if (!uart_irq_rx_ready(my_uart1)) {
		return;
	}
	/* read until FIFO empty */
	while (uart_fifo_read(my_uart1, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';
			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart1_msgq, &rx_buf, K_NO_WAIT);
			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		}
		else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

/*
 * Print a null-terminated string character by character to the UART interface
 */

void print_uart0(char *buf) {
	int msg_len = strlen(buf);
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(my_uart0, buf[i]);
	}
}

void print_uart1(char *buf) {
	int msg_len = strlen(buf);
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(my_uart1, buf[i]);
	}
}

int uart_init(void) {
    int ret; 
	if (!device_is_ready(my_uart0)) {
		printk("UART0 device not found!");
		return -1;
	}
	// configure interrupt and callback to receive data
	ret = uart_irq_callback_user_data_set(my_uart0, uart0_cb, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART0 API support not enabled\n");
		}
		else if (ret == -ENOSYS) {
			printk("UART0 device does not support interrupt-driven API\n");
		}
		else {
			printk("Error setting UART0 callback: %d\n", ret);
		}
		return -1;
	}

	uart_irq_rx_enable(my_uart0);
    return 0;
}

static char buff[1024];

void uart_thread_entry(void *a, void *b, void *c) {
	print_uart0("UART0: Write something and and press enter to send to google sheets:\r\n");
	// print_uart1("UART1: Write something and and press enter to send to uart0:\r\n");

	while (1) {
		/* Wait until there is a message in the uart0_msgq */
		if (k_msgq_get(&uart0_msgq, &tx_buf, K_FOREVER) == 0) {
			printk("mssg from uart0: %s\r\n", tx_buf);
            int ret = snprintf(buff, sizeof(buff), HTTP_POST_MESSAGE, tx_buf);
            ret = snprintf(uart_send, sizeof(uart_send), HTTPS_POST_REGULAR_UPLOAD, 
                                                            HTTPS_TARGET,
                                                            HTTPS_HOSTNAME,
                                                            HTTPS_PORT,
                                                            ret,
                                                            buff);
        
            if (ret > 0 && k_msgq_put(&https_send_queue, uart_send, K_NO_WAIT) == 0) {
                    printk("successfully queued a messaged from uart thread to https thread: \n%s", uart_send);
                };
		}
        /* Give control to other threads to do their thing */
        k_sleep(K_MSEC(100));
	}
}

// void uart_thread_entry(void *a, void *b, void *c) {
// 	while (1) {
// 		if (k_msgq_get(&uart1_msgq, &tx_buf, K_FOREVER) == 0) {
// 			printk("mssg from uart0: %s\r\n", tx_buf);
// 			/* Process the RG-15 icoming data here which is stored in tx_buf */
//             int ret = snprintf(buff, sizeof(buff), HTTP_POST_MESSAGE, tx_buf);
//             ret = snprintf(uart_send, sizeof(uart_send), HTTPS_POST_REGULAR_UPLOAD, 
//                                                             HTTPS_TARGET,
//                                                             HTTPS_HOSTNAME,
//                                                             HTTPS_PORT,
//                                                             ret,
//                                                             buff);
        
//             if (ret > 0 && k_msgq_put(&https_send_queue, uart_send, K_NO_WAIT) == 0) {
//                     printk("successfully queued a messaged from uart thread to https thread: \n%s", uart_send);
//                 };
// 		}
//         /* Give control to other threads to do their thing */
//         k_sleep(K_MSEC(100));
// 	}
// }