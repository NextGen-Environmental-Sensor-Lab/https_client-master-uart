#ifndef DATA_ACQ_H
#define DATA_ACQ_H

#define DATA_ACQ_HTTPS_HOSTNAME  "script.google.com"
#define DATA_ACQ_HTTPS_PORT      "443"
#define DATA_ACQ_HTTPS_TARGET    "/macros/s/AKfycbyYJDElhjQQdPbhONXswdhvUNnWmd6RTAB9eoQavpZkyNH1cietC8S0KZ7HGoI7ytCVtA/exec"

#define DATA_ACQ_POST_PAYLOAD                        \
        "{"                                         \
            "\"command\":\"appendRow\","            \
            "\"sheet_name\":\"Sheet1\","            \
            "\"values\":\"%s,%d,%s\""                  \
        "}"


#define DEFAULT_DATA_ACQ_PERIODICITY 60

<<<<<<< Updated upstream
=======
void parse_data_and_queue_https_message(void);
int data_acq_init(void);

>>>>>>> Stashed changes
#endif