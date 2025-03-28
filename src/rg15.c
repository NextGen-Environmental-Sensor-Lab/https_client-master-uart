#include "rg15.h"
#include <stddef.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "errno.h"
#include "uart_handler.h"

#define RG15_COMMAND_TABLE_LEN 32
#define MAX_RG15_RESPONSE_LEN  512
#define MAX_RG15_UNITS_LEN     16

K_SEM_DEFINE(rg15_response_ready_sem, 0, 1);

//extern const struct device *const my_uart1;

typedef struct rg15_command_t {
    const char *cmd_fmt;
    const char *resp_fmt;
    int (*func)(struct rg15_t *, const char *,const char *);
} rg15_command_t;

static int rg15_acc_data_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_meas_data_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_reset_device_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_baud_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_op_mode_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_resolution_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_units_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_lens_bad_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);
static int rg15_emitter_sat_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp);

/** Look up table for RG15 commands and their expected responses. 
 *  Remove spaces around in the response string before parsing to make it safe with sscanf */
static rg15_command_t const rg15_command_table[RG15_COMMAND_TABLE_LEN] = {
    {"A",       "Acc%lf%s",                                         rg15_acc_data_cb},
    {"R",       "Acc%lf%s,EventAcc%lf%s,TotalAcc%lf%s,RInt%lf%s",   rg15_meas_data_cb},
    {"K",       "Reset%c",                                          rg15_reset_device_cb},
    {NULL,      NULL,                                               NULL}
};

static void clean_rg_15_response(const char *rsp, char *buff, uint32_t len) {
    if (rsp == NULL || buff == NULL) {
        return;
    }

    uint32_t idx = 0;
    char *data_ptr = rsp;

    while(*data_ptr && idx < len - 1) {
		if ((*data_ptr != '\r') && (*data_ptr != '\n') && (*data_ptr != ' ')) {
			buff[idx++] = *data_ptr;
		}
		data_ptr++;
	}
    buff[idx] = '\0';
    printk("Cleaned RG-15 message is [%s]\r\n", buff);
}

static int rg15_acc_data_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    int ret;
    char acc_units[MAX_RG15_UNITS_LEN] = {0};
    char buff[MAX_RG15_RESPONSE_LEN] = {0};

    clean_rg_15_response(recv_rsp, buff, sizeof(buff));
    
    ret = sscanf(buff, exp_rsp, rg15->data.acc, acc_units);
    if (ret != 2) {
        printk("Invalid arguments received. Expected format \"Acc 0.000 in\", got %s\r\n", recv_rsp);
        return -EINVAL;
    }
    /* Todo: validate expected units in rg-15 instance match with received */
    if (strcmp(acc_units, "in") == 0) {

    } else if (strcmp(acc_units, "mm") == 0) {
        
    } else {
        printk("Invalid units found. Expected \"in\" or \"mm\", received %s\r\n", acc_units);
        return -EINVAL;
    }

    return 0;
}

static int rg15_meas_data_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    int ret;
    char acc_units[MAX_RG15_UNITS_LEN] = {0};
    char event_acc_units[MAX_RG15_UNITS_LEN] = {0};
    char total_acc_units[MAX_RG15_UNITS_LEN] = {0};
    char r_int_units[MAX_RG15_UNITS_LEN] = {0};
    char buff[MAX_RG15_RESPONSE_LEN] = {0};

    clean_rg_15_response(recv_rsp, buff, sizeof(buff));
    
    ret = sscanf(buff, exp_rsp, rg15->data.acc, acc_units,
                                rg15->data.event_acc, event_acc_units,
                                rg15->data.total_acc, total_acc_units,
                                rg15->data.r_int, r_int_units);
    if (ret != 8) {
        printk("Invalid arguments received. Expected format \"Acc 0.001 in, EventAcc 0.019 in, TotalAcc 0.019 in, RInt 0.082 iph\", got %s\r\n", recv_rsp);
        return -EINVAL;
    }
    /* Todo: validate expected units in rg-15 instance match with received */
    if (strcmp(acc_units, "in") == 0) {

    } else if (strcmp(acc_units, "mm") == 0) {
        
    } else {
        printk("Invalid units found. Expected \"in\" or \"mm\", received %s\r\n", acc_units);
        return -EINVAL;
    }
    return 0;
}

static void print_reset_reason(const char c) {
    switch (c) {
    case 'N':
        printk("Reset reason: 'N' = Normal Power Up\r\n");
        break;
    case 'M':
        printk("Reset reason: 'M' = Memory Clear (MCLR)\r\n");
        break;
    case 'W':
        printk("Reset reason: 'W' = Watchdog timer reset\r\n");
        break;
    case 'O':
        printk("Reset reason: 'O' = Stack Overflow\r\n");
        break;
    case 'U':
        printk("Reset reason: 'U' = Stack Underflow\r\n");
        break;
    case 'B':
        printk("Reset reason: 'B' = Brownout\r\n");
        break;
    case 'D':
        printk("Reset reason: 'D' = Other. Kill/Reboot command also returns 'D'\r\n");
        break;
    default:
        printk("ERROR: Reset reason not found!\r\n");
        break;
    }
}

static int rg15_reset_device_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    int ret;
    char c;
    char buff[MAX_RG15_RESPONSE_LEN] = {0};

    clean_rg_15_response(recv_rsp, buff, sizeof(buff));
    ret = sscanf(buff, exp_rsp, &c);
    if (ret != -1) {
        printk("ERROR: Expected respose format Reset 'D', got %s", recv_rsp);
        rg15->state = RG15_ERROR;
        return -EINVAL;
    }

    return 0;
}

static int rg15_baud_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

static int rg15_op_mode_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

static int rg15_resolution_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

static int rg15_units_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

static int rg15_lens_bad_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

static int rg15_emitter_sat_cb(struct rg15_t *rg15, const char *exp_rsp, const char *recv_rsp) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

static void rg15_exec_command(struct rg15_t *rg15, const char *cmd) {
    if (rg15 == NULL || cmd == NULL) {
        return -EINVAL;
    }
    int idx;
    int found = 0;

    for (int idx = 0; rg15_command_table[idx].func != NULL; idx++) {
		if (strstr(cmd, rg15_command_table[idx].cmd_fmt) != NULL) {
			found = 1;
			break;
		}
	}
    if (!found) {
        printk("Invalid RG15 command!\r\n");
        return;
    }
    print_uart(my_uart1, cmd);
    k_sem_take(&rg15_response_ready_sem, K_SECONDS(10));
}

/** Initialize RG-15 instance */
int rg15_init(struct rg15_t *rg15,  rg15_mode_t mode, rg15_resolution_t res, rg15_units_t units) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

/** Set Operational mode */
int rg15_set_op_mode(struct rg15_t *rg15, rg15_mode_t mode) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

/** Force set resolution */
int rg15_set_resolution(struct rg15_t *rg15, rg15_resolution_t res) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

/** Force set units */
int rg15_set_units(struct rg15_t *rg15, rg15_units_t units) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

/** Poll a reading from RG-15, also sets to polling mode if not already */
int rg15_poll_reading(struct rg15_t *rg15, rg15_data_t *data) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}

/** Reset RG-15, user must call rg15_init again after this to initialize desired configuration */
int rg15_reset_device(struct rg15_t *rg15) {
    if (rg15 == NULL) {
        return -EINVAL;
    }
    return 0;
}