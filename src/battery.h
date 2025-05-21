#ifndef BATTERY_H
#define BATTERY_H
 
//#include <zephyr/drivers/adc.h>
//#include <stdbool.h>

 #define BATVOLT_R1         (float)4.7                 // MOhm
 #define BATVOLT_R2         (float)10.0                // MOhm
 #define INPUT_VOLT_RANGE   (float)3.6           // Volts
 #define VALUE_RANGE_10_BIT (float)1.023        // (2^10 - 1) / 1000
  
 #define ADC_RESOLUTION 10
 #define ADC_GAIN ADC_GAIN_1_6
 #define ADC_REFERENCE ADC_REF_INTERNAL
 #define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)
 #define ADC_1ST_CHANNEL_ID 0
 #define ADC_1ST_CHANNEL_INPUT SAADC_CH_PSELP_PSELP_AnalogInput0
 
 #define BUFFER_SIZE 1
 
 int get_battery_voltage(uint16_t *battery_voltage);
 
 bool init_adc(void);
 
 #endif // BATTERY_H
 


// #ifndef BATTERY_H_
// #define BATTERY_H_

// /* Enable measurement of the battery voltage */
// int battery_measure_enable(bool enable);
// /* Measure the battery voltage */
// int battery_sample(void);
// int battery_init(void);

// #endif /* APPLICATION_BATTERY_H_ */
