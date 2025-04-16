#include "data_acq.h"
#include "uart_handler.h"
<<<<<<< Updated upstream
=======
#include "https_handler.h"
#include "battery.h"

#include <modem/modem_info.h>

>>>>>>> Stashed changes
#include <zephyr/kernel.h>
#include <ctype.h>

// #define NETWORK_PARAMS_DATA                     \
//                 "\"rssi\": \"%s\","    		\
//                 "\"cb\": \"%s\","       	\
//                 "\"tac\": \"%s\"," 		\
//                 "\"dev_cell_id\": \"%s\""

#define NETWORK_PARAMS_DATA "%s,%s,%s,%s"

/* Semaphore to get a reading. Initial value is 0 and max is 1 */
K_SEM_DEFINE(data_ready_sem, 0, 1);

extern const struct device *const my_uart1;
<<<<<<< Updated upstream

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
=======
extern char rx_buf[MSG_SIZE]; // receive buffer used in UART ISR callback
extern char clean_buff[MSG_SIZE];
extern char buff[1024];
static char uart_send[SEND_BUF_SIZE];
static char network_info[512];

extern struct k_msgq https_send_queue;

K_SEM_DEFINE(data_acq_start_sem, 0, 1);

void data_acq_entry(void *a, void *b, void *c)
{
    printk("Data thread starting...\n");

    /* Wait for the data_acq_start_sem.
     * data_acq_start_sem is given when we are connected to the 
     * netowrk for the first time after boot.    
     */
    k_sem_take(&data_acq_start_sem, K_FOREVER);
    while (1)
    {
>>>>>>> Stashed changes
        printk("Polling measurement from RG-15...\r\n");
        print_uart(my_uart1, "R\r\n");
        k_sleep(K_SECONDS(DEFAULT_DATA_ACQ_PERIODICITY));
    }
<<<<<<< Updated upstream
=======
}

static int prepare_network_info(char *buf, uint32_t max_buf_len) {
	int ret;
	char signal_strength[16];
	char current_band[16];
	char tracking_area_code[32];
	char device_cell_id[32];

	/* Get modem information */
	modem_info_string_get(MODEM_INFO_RSRP, signal_strength,
			sizeof(signal_strength));
	modem_info_string_get(MODEM_INFO_CUR_BAND, current_band,
			sizeof(current_band));
	modem_info_string_get(MODEM_INFO_AREA_CODE, tracking_area_code,
			sizeof(tracking_area_code));
	modem_info_string_get(MODEM_INFO_CELLID, device_cell_id,
			sizeof(device_cell_id));

	/* Format the network parameters string */
	ret = snprintf(buf, max_buf_len, NETWORK_PARAMS_DATA, signal_strength,
			current_band, tracking_area_code, device_cell_id);

	return ret;
}

void parse_data_and_queue_https_message(void)
{
    /* Acc 0.001 in, EventAcc 0.019 in, TotalAcc 0.019 in, RInt 0.082 iph */
    int idx = 0;
    char *data_ptr;
    int ret;

    /* For the rg15 'Acc' is the first word in data string*/
    if (strstr(rx_buf, "Acc") == NULL)
    {
        return;
    }

    data_ptr = &rx_buf[0];
    memset(clean_buff, '\0', sizeof(clean_buff));
    /* parsing the data */
    while (*data_ptr != '\0' && idx < (sizeof(rx_buf) - 1))
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

    printk("Parsed buffer is: %s\r\n", clean_buff);
    
    //int batt_reading = battery_sample();

    uint16_t batt_reading = 0;
    ret = get_battery_voltage(&batt_reading);
    if (ret != 0)
    {
        printk("Failed to read battery voltage: %d\n", ret);
        return;
    }
	/* get network info */
	prepare_network_info(network_info, 512);
	printk("Network info: %s\n", network_info);

    ret = snprintf(buff, sizeof(buff), DATA_ACQ_POST_PAYLOAD, clean_buff, batt_reading, network_info);
    ret = snprintf(uart_send, sizeof(uart_send), HTTPS_POST_REQUEST,
                   DATA_ACQ_HTTPS_TARGET,
                   DATA_ACQ_HTTPS_HOSTNAME,
                   DATA_ACQ_HTTPS_PORT,
                   ret,  // length of the payload
                   buff); // payload

    if (ret > 0 && k_msgq_put(&https_send_queue, uart_send, K_NO_WAIT) == 0)
    {
        printk("Successfully PARSED and QUEUED from UART thread to HTTPS thread: \n%s\n", uart_send);
    };
}

int data_acq_init(void)
{
    printk("Force rebooting RG-15...\r\n");
    print_uart(my_uart1, "K\n");
    /* Give it a couple of seconds after reboot */
    k_busy_wait(3 * 1000 * 1000);
    printk("Putting rg15 into poll mode...\r\n");
    print_uart(my_uart1, "P\r\n");
    k_busy_wait(1 * 1000 * 1000);

    return 0;
>>>>>>> Stashed changes
}