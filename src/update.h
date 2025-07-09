#ifndef UPDATE_H
#define UPDATE_H

#define OTA_TLS_SEC_TAG 23459

#define CONFIG_DOWNLOAD_CLIENT_MAX_HOSTNAME_SIZE 255
#define CONFIG_DOWNLOAD_CLIENT_MAX_FILENAME_SIZE 255

/* states that the update process can be in
 *
 * IDLE: when the network is disconnected
 * and the rest are self-explanatory */
enum fota_state {
        IDLE,
        CONNECTED,
        UPDATE_DOWNLOAD,
        UPDATE_PENDING,
        UPDATE_APPLY
};

/* apply_ota_state function updates the state of the
 * ota helper instance whenever there is a change in
 * the state of the network. This is done in the file
 * lte_handler.c as a callback to the network changes
 * */
void apply_ota_state(enum fota_state new_state);

/* Initializes the OTA handler */
void ota_update_init(void);

#endif
