#ifndef SENSOR_DATA_ACQ_H
#define SENSOR_DATA_ACQ_H

// #define SLEEP_TIME_MS 1000
// #define LED0_NODE DT_ALIAS(led0)

#define DEFAULT_DATA_ACQ_PERIODICITY 60

void parse_sensor_and_queue_https_message();

#endif