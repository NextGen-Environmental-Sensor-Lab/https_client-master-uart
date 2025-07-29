#ifndef DATA_ACQ_H
#define DATA_ACQ_H

#define DATA_ACQ_HTTPS_HOSTNAME "ngens.environmentalcrossroads.net"
#define DATA_ACQ_HTTPS_PORT     "443"
#define DATA_ACQ_HTTPS_TARGET   "/nodered/globe"

/* Timestamp, Cleaned RRG-15 data, Battery mVolts, Netowrk info */
#define DATA_ACQ_POST_PAYLOAD                                                                                          \
        "{"                                                                                                            \
        "\"msg_type\":\"data_acq\","                                                                                   \
        "\"values\":\"%s,%s,%d,%s\""                                                                                   \
        "}"

#define CFG_POST_PAYLOAD                                                                                               \
        "{"                                                                                                            \
        "\"msg_type\":\"cfg\","                                                                                        \
        "\"rst_reas\": \"%s\","                                                                                        \
        "\"app_fw_ver\": \"%s\","                                                                                      \
        "\"modem_fw_ver\": \"%s\","                                                                                    \
        "}"

#define DEFAULT_DATA_ACQ_PERIODICITY 60 // 1 minutes

void parse_data_and_queue_https_message(void);
int data_acq_init(void);

#endif
