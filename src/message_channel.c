/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

LOG_MODULE_REGISTER(message_channel, CONFIG_LOG_DEFAULT_LEVEL);

void error_callback(const struct zbus_channel *chan);

ZBUS_LISTENER_DEFINE(error, error_callback);

/* HTTPS thread attempts to clear any
 * outstanding messages upon receiving a trigger
 */
ZBUS_CHAN_DEFINE(HTTPS_TRIGGER_CHAN,                  /* Name */
                 int,                                 /* Message type */
                 NULL,                                /* Validator */
                 NULL,                                /* User data */
                 ZBUS_OBSERVERS(mqtt_thread_channel), /* Observers */
                 ZBUS_MSG_INIT(0)                     /* Initial value {0} */
);

/* LTE handler thread updates LTE conn status so that HTTPS thread
 * can update it's internal state accordingly.
 */
ZBUS_CHAN_DEFINE(NETWORK_CHAN,                        /* Name */
                 enum network_status,                 /* Message type */
                 NULL,                                /* Validator */
                 NULL,                                /* User data */
                 ZBUS_OBSERVERS(mqtt_thread_channel), /* Observers */
                 ZBUS_MSG_INIT(0)                     /* Initial value {0} */
);

/* Reboots the system in case of a fatal failure
 */
ZBUS_CHAN_DEFINE(FATAL_ERROR_CHAN,      /* Name */
                 int,                   /* Message type */
                 NULL,                  /* Validator */
                 NULL,                  /* User data */
                 ZBUS_OBSERVERS(error), /* Observers */
                 ZBUS_MSG_INIT(0)       /* Initial value {0} */
);

void error_callback(const struct zbus_channel *chan) {
        if (&FATAL_ERROR_CHAN == chan) {
                if (IS_ENABLED(CONFIG_HTTPS_SAMPLE_ERROR_REBOOT_ON_FATAL)) {
                        printk("FATAL error, rebooting");
                        LOG_PANIC();
                        sys_reboot(0);
                }
        }
}
