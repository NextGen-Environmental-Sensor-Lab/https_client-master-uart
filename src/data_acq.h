#ifndef DATA_ACQ_H
#define DATA_ACQ_H

#define DATA_ACQ_HTTPS_HOSTNAME  "ngens.environmentalcrossroads.net"
#define DATA_ACQ_HTTPS_PORT      "443"
#define DATA_ACQ_HTTPS_TARGET    "/nodered/globe" // "/macros/s/AKfycbySi8NPaVAqbAu-arNrJHKaa-xWFXxtQ-7oXr7T_Rc-yW2kUoH4YICrbvguGLcpjgsP/exec"

/* Timestamp, Cleaned RRG-15 data, Battery mVolts, Netowrk info */
#define DATA_ACQ_POST_PAYLOAD                       \
        "{"                                         \
            "\"command\":\"appendRow\","            \
            "\"sheet_name\":\"Sheet1\","            \
            "\"values\":\"%s,%s,%d,%s\""            \
        "}"

#define DEFAULT_DATA_ACQ_PERIODICITY 600 // 10 minutes

void parse_data_and_queue_https_message(void);
int data_acq_init(void);

#endif