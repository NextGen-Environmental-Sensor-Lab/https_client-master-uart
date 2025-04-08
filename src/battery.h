#ifndef BATTERY_H_
#define BATTERY_H_

/*Enable measurement of the battery voltage*/
int battery_measure_enable(bool enable);

/*Measure the battery voltage*/
int battery_sample(void);

#endif /* APPLICATION_BATTERY_H_ */
