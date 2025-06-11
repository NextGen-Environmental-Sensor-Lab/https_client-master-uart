#include "update.h"

#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <net/fota_download.h>
#include <ssp/stdio.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/parser_url.h>
#include <zephyr/sys/reboot.h>

/* flag to let uart_handler know that the
 * ota handler is ready. Used to prevent
 * premature invocation of the ota handler
 * which might be fatal */
bool ota_handler_ready       = false;

extern bool ota_session_in_progress;

static enum fota_state state = IDLE;
static struct k_work fota_work;

/* private function prototypes */
static void fota_work_cb(struct k_work *work);
static int update_download(void);
static int apply(void);

extern char ota_hostname[CONFIG_DOWNLOAD_CLIENT_MAX_HOSTNAME_SIZE];
extern char ota_filename[CONFIG_DOWNLOAD_CLIENT_MAX_HOSTNAME_SIZE];

/* apply_ota_state is a callback function to
 * update the state of the ota handler */
void apply_ota_state(enum fota_state new_state) {
        if (state == new_state) {
                return;
        }

        state = new_state;
        switch (new_state) {
        case IDLE:
                break;
        case CONNECTED:
                printk("OTA handler is ready\n");
                ota_handler_ready = true;
                break;
        case UPDATE_DOWNLOAD:
                k_work_submit(&fota_work);
                break;
        case UPDATE_PENDING:
                apply();
                break;
        case UPDATE_APPLY:
                k_work_submit(&fota_work);
                break;
        }
}

static int ota_cert_provision(void) {
        static const char cert[] = {
#include "../cert/GithubRootCA1" //AmazonRootCA1"
        };
        BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");

#if CONFIG_MODEM_KEY_MGMT
        bool exists;
        int mismatch;
        int err;

        /* It may be sufficient for you application to check whether the correct
         * certificate is provisioned with a given tag directly using modem_key_mgmt_cmp().
         * Here, for the sake of the completeness, we check that a certificate exists
         * before comparing it with what we expect it to be.
         */
        err = modem_key_mgmt_exists(OTA_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists);
        if (err) {
                printk("Failed to check for certificates err %d\n", err);
                return err;
        }

        if (exists) {
                mismatch = modem_key_mgmt_cmp(OTA_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert, sizeof(cert));
                if (!mismatch) {
                        printk("Certificate match\n");
                        return 0;
                }

                printk("Certificate mismatch\n");
                err = modem_key_mgmt_delete(OTA_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
                if (err) {
                        printk("Failed to delete existing certificate, err %d\n", err);
                }
        }

        printk("Provisioning certificate to the modem\n");

        /*  Provision certificate to the modem */
        err = modem_key_mgmt_write(OTA_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert, sizeof(cert));
        if (err) {
                printk("Failed to provision certificate, err %d\n", err);
                return err;
        }
#else  /* CONFIG_MODEM_KEY_MGMT */
        err = tls_credential_add(OTA_TLS_SEC_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, cert, sizeof(cert));
        if (err == -EEXIST) {
                printk("CA certificate already exists, sec tag: %d\n", TLS_SEC_TAG);
        } else if (err < 0) {
                printk("Failed to register CA certificate: %d\n", err);
                return err;
        }
#endif /* !CONFIG_MODEM_KEY_MGMT */

        return 0;
}

static void fota_work_cb(struct k_work *work) {
        int err;

        ARG_UNUSED(work);

        switch (state) {
        case UPDATE_DOWNLOAD:
                err = update_download();
                if (err) {
                        printk("Download failed, err %d\n", err);
                        apply_ota_state(CONNECTED);
                }
                break;
        case UPDATE_APPLY:
                lte_lc_power_off();
                sys_reboot(SYS_REBOOT_WARM);
                break;
        default:
                break;
        }
}

static int apply(void) {
        if (state != UPDATE_PENDING) {
                return -EPERM;
        }

        apply_ota_state(UPDATE_APPLY);

        return 0;
}

static void fota_dl_handler(const struct fota_download_evt *evt) {
        switch (evt->id) {
        case FOTA_DOWNLOAD_EVT_PROGRESS:
                ota_session_in_progress = true;
                break;
        case FOTA_DOWNLOAD_EVT_ERROR:
                printk("Received error from fota_download\n");
                apply_ota_state(CONNECTED);
                ota_session_in_progress = false;
                break;
        case FOTA_DOWNLOAD_EVT_FINISHED:
                apply_ota_state(UPDATE_PENDING);
                break;
        default:
                break;
        }
}

static int update_download(void) {
        int err;

        err = fota_download_init(fota_dl_handler);
        if (err) {
                printk("fota_download_init() failed, err %d\n", err);
                return 0;
        }

        printk("ota_hostname is: %s\r\n", ota_hostname);
        printk("ota_filename is: %s\r\n", ota_filename);

        /* Functions for getting the host and file */
        err = fota_download_start(ota_hostname, ota_filename, OTA_TLS_SEC_TAG, 0, 0);
        if (err) {
                printk("fota_download_start() failed, err %d\n", err);
                return err;
        }

        return 0;
}

void ota_update_init() {
        int err;

        /* This is needed so that MCUBoot won't revert the update */
        boot_write_img_confirmed();

        apply_ota_state(IDLE);

        k_work_init(&fota_work, fota_work_cb);

        err = ota_cert_provision();
        if (err) {
                printk("Could not provision root CA to %d", OTA_TLS_SEC_TAG);
        }
}