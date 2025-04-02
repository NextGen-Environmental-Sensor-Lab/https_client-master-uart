#ifndef BATTERY_H_
#define BATTERY_H_

/*Enable measurement of the battery voltage*/
int battery_measure_enable(bool enable);

/*Measure the battery voltage*/
int battery_sample(void);

struct battery_level_point {
	/** Remaining life at #lvl_mV. */
	uint16_t lvl_pptt;

	/** Battery voltage at #lvl_pptt remaining life. */
	uint16_t lvl_mV;
};


/* Calculate the estimated battery level based on a measured voltage.*/
unsigned int battery_level_pptt(unsigned int batt_mV,
    const struct battery_level_point *curve);
#endif /* APPLICATION_BATTERY_H_ */
