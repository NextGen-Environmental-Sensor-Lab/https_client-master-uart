#ifndef RG15_H_
#define RG15_H_

#include <stdint.h>
#include <stdbool.h>

/** Enum to define RG-15 states */
typedef enum {
    RG15_RESET = 0,
    RG15_SETUP_DONE,
    RG15_ERROR
} rg15_state_t;

/** Enum to define RG-15 operational modes */
typedef enum {
    RG15_MODE_POLLING = 0,
    RG15_MODE_CONTINUOUS
} rg15_mode_t;

/** Enum to define RG-15 resolution */
typedef enum {
    RG15_DATA_HIGH_RES = 0,
    RG15_DATA_LOW_RES
} rg15_resolution_t;

/** Enum to define RG-15 units */
typedef enum {
    RG15_UNITS_IMPERIAL = 0,
    RG15_UNITS_METRIC
} rg15_units_t;

/** Structure to define RG-15 data format */
typedef struct rg15_data_t {
    double acc;
    double event_acc;
    double total_acc;
    double r_int;
} rg15_data_t;

/** Structure to define RG-15 instance */
typedef struct rg15_t {
    rg15_state_t state;     /*< Current rg15 state */
    rg15_mode_t mode;       /*< Operational mode */
    rg15_resolution_t res;  /*< Resolution can be forced high or low */
    rg15_units_t units;     /*< Measurement units can be forced imperial or metric */
    bool lens_bad;          /*< Lens is not able to get sufficient light for reasonable readings */
    bool emitter_sat;       /*< Emitter is saturated */
    uint32_t baud;          /*< current baud rate, default is 9600 */
    rg15_data_t data;       /*< data buffer to hold readings */
} rg15_t;

/** Initialize RG-15 instance */
int rg15_init(struct rg15_t *rg15,  rg15_mode_t mode, rg15_resolution_t res, rg15_units_t units);

/** Set Operational mode */
int rg15_set_op_mode(struct rg15_t *rg15, rg15_mode_t mode);

/** Force set resolution */
int rg15_set_resolution(struct rg15_t *rg15, rg15_resolution_t res);

/** Force set units */
int rg15_set_units(struct rg15_t *rg15, rg15_units_t units);

/** Poll a reading from RG-15, also sets to polling mode if not already */
int rg15_poll_reading(struct rg15_t *rg15, rg15_data_t *data);

/** Reset RG-15, user must call rg15_init again after this to initialize desired configuration */
int rg15_reset_device(struct rg15_t *rg15);

#endif