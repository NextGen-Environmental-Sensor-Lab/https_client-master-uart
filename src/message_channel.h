#ifndef MESSAGE_CHANNEL_H__
#define MESSAGE_CHANNEL_H__

#include <zephyr/kernel.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(MQTT_TRIGGER_CHAN, NETWORK_CHAN, FATAL_ERROR_CHAN);

/** @brief Macro used to send a message on the FATAL_ERROR_CHANNEL.
 *	   The message will be handled in the error module.
 */
#define SEND_FATAL_ERROR()                                                                         \
        int not_used = -1;                                                                         \
        if (zbus_chan_pub(&FATAL_ERROR_CHAN, &not_used, K_SECONDS(10))) {                          \
                printk("Sending a message on the fatal error channel failed, rebooting");          \
                LOG_PANIC();                                                                       \
                IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)));                                        \
        }

enum network_status {
        NETWORK_DISCONNECTED,
        NETWORK_CONNECTED,
};

#endif
