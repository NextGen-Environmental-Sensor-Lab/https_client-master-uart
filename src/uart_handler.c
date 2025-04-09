#include "uart_handler.h"
#include "https_handler.h"
#include "data_acq.h"
#include "battery.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <math.h>
#include <string.h>
#include <stdio.h>

#define DEV_OTHER DT_NODELABEL(uart1)


extern struct k_sem get_reading_sem;

// char uart_send[SEND_BUF_SIZE];

// static bool rg_15_setup_done = false;

extern struct k_msgq https_send_queue;
extern struct k_sem rg15_ready_sem;

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart1_msgq, MSG_SIZE, 10, 4);

const struct device *const my_uart1 = DEVICE_DT_GET(DEV_OTHER);

/* receive buffer used in UART ISR callback */
char tx_buf[MSG_SIZE];
char clean_buff[MSG_SIZE];
static char rx_buf1[MSG_SIZE];
static int rx_buf_pos1;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void uart_cb(const struct device *dev, void *user_data) {
	uint8_t c;
	int *pos;
	char *buff; 
	struct k_msgq *msgq;
	
	if (!uart_irq_update(dev)) {	
		return;
	}
	if (!uart_irq_rx_ready(dev)) {	
		return;
	}

	pos = &rx_buf_pos1;
	buff = rx_buf1;
	msgq = &uart1_msgq;
	
	/* read until FIFO empty */
	while (uart_fifo_read(dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && *pos > 0) {
			/* terminate string */
			buff[*pos] = '\0';
			/* if queue is full, message is silently dropped */
			k_msgq_put(msgq, buff, K_NO_WAIT);
			/* reset the buffer (it was copied to the msgq) */
			*pos = 0;
		}
		else if (*pos < MSG_SIZE - 1) {
			buff[*pos] = c;
			*pos = *pos + 1;
		}
	}
}

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(const struct device *dev, char *buf) {
	int msg_len = strlen(buf);
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(dev, buf[i]);
	}
}

int uart_init(const struct device *dev) {
    
	printk("UART init started....\r\n");

	if (dev == NULL) {
		return -1;
	}

	int ret; 
	if (!device_is_ready(dev)) {
		printk("%s device is not ready!",  "UART1");
		return -1;
	}
	// configure interrupt and callback to receive data
	ret = uart_irq_callback_user_data_set(dev, uart_cb, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART1 API support not enabled\n");
		}
		else if (ret == -ENOSYS) {
			printk("UART1 device does not support interrupt-driven API\n");
		}
		else {
			printk("Error setting UART1 callback: %d\n", ret);
		}
		return -1;
	}

	uart_irq_rx_enable(dev);
    return 0;
}

char buff[1024];

// 

void uart_thread_entry(void *a, void *b, void *c) {

	printk("UART thread starting...\n");
	k_sem_give(&rg15_ready_sem);
	while (1) {
		/* Check if there is a message in the uart1_msgq
		 * RG-15 messages are received via UART1 
		 */
		if (k_msgq_get(&uart1_msgq, &tx_buf, K_NO_WAIT) == 0) {
			printk("mssg from uart1: %s\r\n", tx_buf);	
			parse_rg15_and_queue_https_message();
		}
        /* Give control to other threads to do their thing */
        k_sleep(K_MSEC(100));
	}
}