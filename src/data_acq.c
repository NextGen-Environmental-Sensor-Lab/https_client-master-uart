#include "data_acq.h"
#include "uart_handler.h"
#include "https_handler.h"
#include "battery.h"

#include <date_time.h>
#include <modem/modem_info.h>
#include <zephyr/logging/log.h>

#include <zephyr/kernel.h>
#include <ctype.h>

/* signal_strength, ip, iccid, imei, current_band, tracking_area_code, device_cell_id */
#define NETWORK_PARAMS_DATA "%s,%s,%s,%s,%s,%s,%s"

static char buff[1024];

/* Semaphore to get a reading. Initial value is 0 and max is 1 */
K_SEM_DEFINE(data_ready_sem, 0, 1);

#define MSG_SIZE 512

LOG_MODULE_REGISTER(data_acq, 3);

extern const struct device *const my_uart1;
extern char rx_buf[MSG_SIZE]; // receive buffer used in UART ISR callback
extern char clean_buff[MSG_SIZE];
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
        printk("Request measurement from RG-15...\r\n");
        print_uart(my_uart1, "R\r\n");
        
        k_sleep(K_SECONDS(DEFAULT_DATA_ACQ_PERIODICITY));
    }
}

static int prepare_network_info(char *buf, uint32_t max_buf_len) {
	int ret;
	char signal_strength[16] = {0};
    char ip[32] = {0};
    char iccid[32] = {0};
	char imei[32] = {0};
	char current_band[16] = {0};
	char tracking_area_code[32] = {0};
	char device_cell_id[32] = {0};

	/* Get modem information */
	modem_info_string_get(MODEM_INFO_RSRP, signal_strength,
			sizeof(signal_strength));
    modem_info_string_get(MODEM_INFO_IP_ADDRESS, ip,
            sizeof(ip));
    modem_info_string_get(MODEM_INFO_ICCID, iccid, 
            sizeof(iccid));
    modem_info_string_get(MODEM_INFO_IMEI, imei,
            sizeof(imei));        
	modem_info_string_get(MODEM_INFO_CUR_BAND, current_band,
			sizeof(current_band));
	modem_info_string_get(MODEM_INFO_AREA_CODE, tracking_area_code,
			sizeof(tracking_area_code));
	modem_info_string_get(MODEM_INFO_CELLID, device_cell_id,
			sizeof(device_cell_id));
    

	/* Format the network parameters string */
	ret = snprintf(buf, max_buf_len, NETWORK_PARAMS_DATA, signal_strength, ip, iccid, imei,
			current_band, tracking_area_code, device_cell_id);

	return ret;
}

static void int64_to_str(int64_t num, char *str) {
	char temp[21];       // Temporary buffer to hold the reversed string
	int i = 0;           // Index for the temporary buffer
	int is_negative = 0; // Flag to indicate if the number is negative

	// Check if the number is negative
	if (num < 0) {
		is_negative = 1;
		num = -num; // Make the number positive for processing
	}

	// Extract each digit and store it in the temporary buffer
	do {
		temp[i++] =
			(num % 10) + '0'; // Convert the digit to its ASCII representation
		num /= 10;
	} while (num > 0);

	// Add the negative sign if the number was negative
	if (is_negative) {
		temp[i++] = '-';
	}

	// Reverse the string into the final output buffer
	int j;
	for (j = 0; j < i; j++) {
		str[j] = temp[i - j - 1];
	}
	str[j] = '\0'; // Null-terminate the string
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
    printf("Parsed buffer is: %s\r\n", clean_buff);
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

    int64_t time_now;
    char time_now_str[21] = {0};

    date_time_now(&time_now); /* P added this */
	int64_to_str(time_now, time_now_str);

    ret = snprintf(buff, sizeof(buff), DATA_ACQ_POST_PAYLOAD, time_now_str, clean_buff, batt_reading, network_info);
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
    // printk("Putting rg15 into poll mode...\r\n");
    // print_uart(my_uart1, "P\r\n");
    // k_busy_wait(1 * 1000 * 1000);
    printk("Set rg15 into continuous mode...\r\n");
    print_uart(my_uart1, "C\r\n");
    k_busy_wait(1 * 1000 * 1000);

    return 0;
}