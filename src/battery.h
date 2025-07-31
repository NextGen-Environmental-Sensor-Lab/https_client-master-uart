#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#define BATVOLT_R1            (float)100  // KOhm
#define BATVOLT_R2            (float)100  // KOhm
#define INPUT_VOLT_RANGE      (float)3.6   // Volts
#define VALUE_RANGE_10_BIT    (float)1.023 // (2^10 - 1) / 1000

#define ADC_RESOLUTION        10
#define ADC_GAIN              ADC_GAIN_1_6
#define ADC_REFERENCE         ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME  ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)
#define ADC_1ST_CHANNEL_ID    7
#define ADC_1ST_CHANNEL_INPUT SAADC_CH_PSELP_PSELP_AnalogInput7

#define BUFFER_SIZE           1

int get_battery_voltage(uint16_t *battery_voltage);

bool init_adc(void);

#endif // BATTERY_H
