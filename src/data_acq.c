#include "data_acq.h"
#include "uart_handler.h"
#include "https_handler.h"
#include "battery.h"

#include <zephyr/kernel.h>

/* Semaphore to get a reading. Initial value is 0 and max is 1 */
K_SEM_DEFINE(rg15_ready_sem, 0, 1);

extern const struct device *const my_uart1;
extern char tx_buf[MSG_SIZE];
extern char clean_buff[MSG_SIZE];
extern char buff[1024];
static char uart_send[SEND_BUF_SIZE];
extern struct k_msgq https_send_queue;

void data_acq_entry(void *a, void *b, void *c)
{
    printk("Data thread starting...\n");

    k_sem_take(&rg15_ready_sem, K_FOREVER);

    printk("Force rebooting RG-15...\r\n");
    print_uart(my_uart1, "K\n");

    /* Give it a couple of seconds after reboot */
    k_busy_wait(3 * 1000 * 1000);
    print_uart(my_uart1, "P\r\n");
    k_busy_wait(1 * 1000 * 1000);

    while (1)
    {
        printk("Polling measurement from RG-15...\r\n");
        print_uart(my_uart1, "R\r\n");
        k_sleep(K_SECONDS(DEFAULT_DATA_ACQ_PERIODICITY));
    }
}

void parse_rg15_and_queue_https_message()
{
    /* Acc 0.001 in, EventAcc 0.019 in, TotalAcc 0.019 in, RInt 0.082 iph */
    int idx = 0;
    char *data_ptr;
    int ret;

    if (strstr(tx_buf, "Acc") == NULL)
    {
        return;
    }

    data_ptr = &tx_buf[0];
    memset(clean_buff, '\0', sizeof(clean_buff));
    while (*data_ptr != '\0' && idx < (sizeof(tx_buf) - 1))
    {
        if ((*data_ptr != '\r') && (*data_ptr != '\n') && (*data_ptr != ' '))
        {
            if (isdigit(*data_ptr) || (*data_ptr == '.') || (*data_ptr == ','))
            {
                clean_buff[idx] = *data_ptr;
                idx++;
            }
        }
        data_ptr++;
    }
    clean_buff[idx] = '\0';

    printk("cleaned buffer is: %s\r\n", clean_buff);
    int batt_reading = battery_sample();
    ret = snprintf(buff, sizeof(buff), HTTP_POST_MESSAGE, clean_buff, batt_reading);
    ret = snprintf(uart_send, sizeof(uart_send), HTTPS_POST_REGULAR_UPLOAD,
                   HTTPS_TARGET,
                   HTTPS_HOSTNAME,
                   HTTPS_PORT,
                   ret,
                   buff);

    if (ret > 0 && k_msgq_put(&https_send_queue, uart_send, K_NO_WAIT) == 0)
    {
        printk("successfully queued a messaged from uart thread to https thread: \n%s", uart_send);
    };
}