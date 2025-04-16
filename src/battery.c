/*
 * Copyright (c) 2019-2022 Actinius
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include <zephyr/drivers/adc.h>
 #include "battery.h"
 
 #define ADC_NODE DT_NODELABEL(adc)
 static int16_t m_sample_buffer[BUFFER_SIZE];
 static const struct device *adc_dev;
 
 static const struct adc_channel_cfg m_1st_channel_cfg = {
	 .gain = ADC_GAIN,
	 .reference = ADC_REFERENCE,
	 .acquisition_time = ADC_ACQUISITION_TIME,
	 .channel_id = ADC_1ST_CHANNEL_ID,
	 .input_positive   = ADC_1ST_CHANNEL_INPUT,
 };
 
 int get_battery_voltage(uint16_t *battery_voltage)
 {
	 int err;
	 const struct adc_sequence sequence = {
		 .channels = BIT(ADC_1ST_CHANNEL_ID),
		 .buffer = m_sample_buffer,
		 .buffer_size = sizeof(m_sample_buffer), // in bytes!
		 .resolution = ADC_RESOLUTION,
	 };
 
	 if (!adc_dev) {
		 return -1;
	 }
 
	 err = adc_read(adc_dev, &sequence); // THIS IS IT !!
	 if (err) {
		 printk("ADC read err: %d\n", err);
		 return err;
	 }
 
	 float sample_value = 0;
	 for (int i = 0; i < BUFFER_SIZE; i++) {
		 sample_value += (float) m_sample_buffer[i];
	 }
	 sample_value /= BUFFER_SIZE;
 
	 *battery_voltage = (uint16_t)(sample_value * (INPUT_VOLT_RANGE / VALUE_RANGE_10_BIT) * ((BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2));
 
	 return 0;
 }
 
 bool init_adc(void)
 {
	 int err;
 
	 adc_dev = DEVICE_DT_GET(ADC_NODE);
	 if (!adc_dev) {
		 printk("Error getting adc failed\n");
		 return false;
	 }
 
	 err = adc_channel_setup(adc_dev, &m_1st_channel_cfg);
	 if (err) {
		 printk("Error in adc setup: %d\n", err);
		 return false;
	 }
	 return true;
 }
 

<<<<<<< Updated upstream
int battery_measure_enable(bool enable)
{
	int rc = -ENOENT;
=======

// #include <math.h>
// #include <stdio.h>
// #include <stdlib.h>

// #include <zephyr/kernel.h>
// #include <zephyr/init.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/drivers/adc.h>
// #include <zephyr/drivers/sensor.h>
// #include <zephyr/logging/log.h>
>>>>>>> Stashed changes

// #include "battery.h"

// LOG_MODULE_REGISTER(BATTERY, CONFIG_ADC_LOG_LEVEL);

// #define VBATT DT_PATH(vbatt)
// #define ZEPHYR_USER DT_PATH(zephyr_user)

// #ifdef CONFIG_BOARD_THINGY52_NRF52832
// /* This board uses a divider that reduces max voltage to
//  * reference voltage (600 mV).
//  */
// #define BATTERY_ADC_GAIN ADC_GAIN_1
// #else
// /* Other boards may use dividers that only reduce battery voltage to
//  * the maximum supported by the hardware (3.6 V)
//  */
// #define BATTERY_ADC_GAIN ADC_GAIN_1_6
// #endif

// struct io_channel_config
// {
// 	uint8_t channel;
// };

// struct divider_config
// {
// 	struct io_channel_config io_channel;
// 	struct gpio_dt_spec power_gpios;
// 	/* output_ohm is used as a flag value: if it is nonzero then
// 	 * the battery is measured through a voltage divider;
// 	 * otherwise it is assumed to be directly connected to Vdd.
// 	 */
// 	uint32_t output_ohm;
// 	uint32_t full_ohm;
// };

// static const struct divider_config divider_config = {
// #if DT_NODE_HAS_STATUS_OKAY(VBATT)
// 	.io_channel = {
// 		DT_IO_CHANNELS_INPUT(VBATT),
// 	},
// 	.power_gpios = GPIO_DT_SPEC_GET_OR(VBATT, power_gpios, {}),
// 	.output_ohm = DT_PROP(VBATT, output_ohms),
// 	.full_ohm = DT_PROP(VBATT, full_ohms),
// #else  /* /vbatt exists */
// 	.io_channel = {
// 		DT_IO_CHANNELS_INPUT(ZEPHYR_USER),
// 	},
// #endif /* /vbatt exists */
// };

// struct divider_data
// {
// 	const struct device *adc;
// 	struct adc_channel_cfg adc_cfg;
// 	struct adc_sequence adc_seq;
// 	int16_t raw;
// };

// static struct divider_data divider_data = {
// #if DT_NODE_HAS_STATUS_OKAY(VBATT)
// 	.adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(VBATT)),
// #else
// 	.adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(ZEPHYR_USER)),
// #endif
// };

// static int divider_setup(void)
// {
// 	const struct divider_config *cfg = &divider_config;
// 	const struct io_channel_config *iocp = &cfg->io_channel;
// 	const struct gpio_dt_spec *gcp = &cfg->power_gpios;
// 	struct divider_data *ddp = &divider_data;
// 	struct adc_sequence *asp = &ddp->adc_seq;
// 	struct adc_channel_cfg *accp = &ddp->adc_cfg;
// 	int rc;

// 	if (!device_is_ready(ddp->adc))
// 	{
// 		LOG_ERR("ADC device is not ready %s", ddp->adc->name);
// 		return -ENOENT;
// 	}

// 	if (gcp->port)
// 	{
// 		if (!device_is_ready(gcp->port))
// 		{
// 			LOG_ERR("%s: device not ready", gcp->port->name);
// 			return -ENOENT;
// 		}
// 		rc = gpio_pin_configure_dt(gcp, GPIO_OUTPUT_INACTIVE);
// 		if (rc != 0)
// 		{
// 			LOG_ERR("Failed to control feed %s.%u: %d",
// 					gcp->port->name, gcp->pin, rc);
// 			return rc;
// 		}
// 	}

// 	*asp = (struct adc_sequence){
// 		.channels = BIT(0),
// 		.buffer = &ddp->raw,
// 		.buffer_size = sizeof(ddp->raw),
// 		.oversampling = 4,
// 		.calibrate = true,
// 	};

// #ifdef CONFIG_ADC_NRFX_SAADC
// 	*accp = (struct adc_channel_cfg){
// 		.gain = BATTERY_ADC_GAIN,
// 		.reference = ADC_REF_INTERNAL,
// 		.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
// 	};

// 	if (cfg->output_ohm != 0)
// 	{
// 		accp->input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + iocp->channel;
// 	}
// 	else
// 	{
// 		accp->input_positive = SAADC_CH_PSELP_PSELP_VDD;
// 	}

// 	asp->resolution = 14;
// #else /* CONFIG_ADC_var */
// #error Unsupported ADC
// #endif /* CONFIG_ADC_var */

// 	rc = adc_channel_setup(ddp->adc, accp);
// 	LOG_INF("Setup AIN%u got %d", iocp->channel, rc);

// 	return rc;
// }

// static bool battery_ok;

// static int battery_setup(void)
// {
// 	int rc = divider_setup();

// 	battery_ok = (rc == 0);
// 	LOG_INF("Battery setup: %d %d", rc, battery_ok);
// 	return rc;
// }

// SYS_INIT(battery_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

// int battery_measure_enable(bool enable)
// {
// 	int rc = -ENOENT;

// 	if (battery_ok)
// 	{
// 		const struct gpio_dt_spec *gcp = &divider_config.power_gpios;

// 		rc = 0;
// 		if (gcp->port)
// 		{
// 			rc = gpio_pin_set_dt(gcp, enable);
// 		}
// 	}
// 	return rc;
// }

// int battery_sample(void)
// {
// 	int rc = -ENOENT;

// 	if (battery_ok)
// 	{
// 		struct divider_data *ddp = &divider_data;
// 		const struct divider_config *dcp = &divider_config;
// 		struct adc_sequence *sp = &ddp->adc_seq;

// 		rc = adc_read(ddp->adc, sp);
// 		sp->calibrate = false;
// 		if (rc == 0)
// 		{
// 			int32_t val = ddp->raw;

// 			adc_raw_to_millivolts(adc_ref_internal(ddp->adc),
// 								  ddp->adc_cfg.gain,
// 								  sp->resolution,
// 								  &val);

// 			if (dcp->output_ohm != 0)
// 			{
// 				rc = val * (uint64_t)dcp->full_ohm / dcp->output_ohm;
// 				LOG_INF("raw %u ~ %u mV => %d mV\n", ddp->raw, val, rc);
// 			}
// 			else
// 			{
// 				rc = val;
// 				LOG_INF("raw %u ~ %u mV\n", ddp->raw, val);
// 			}
// 		}
// 	}
// 	return rc;
// }

// int battery_init(void)
// {
// 	int rc = battery_measure_enable(true);
// 	if (rc != 0)
// 	{
// 		printk("Failed initialize battery measurement: %d\n", rc);
// 		return 0;
// 	}

// 	int batt_mV = battery_sample();
// 	if (batt_mV < 0)
// 	{
// 		printk("Failed to read battery voltage: %d\n", batt_mV);
// 	}

// 	printk("%d mV\n", batt_mV);
// 	return rc;
// }
