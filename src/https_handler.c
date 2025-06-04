#include "https_handler.h"
#include "data_acq.h"
#include "main.h"
#include "update.h"

#include <cerrno>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/smf.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/zbus/zbus.h>

#if defined(CONFIG_POSIX_API)
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#endif

#if CONFIG_MODEM_KEY_MGMT
#include <modem/modem_key_mgmt.h>
#endif

#define LTE_NETWORK_CONN_TIMEOUT_MINUTES 5 /* change to 30 minutes in the field */

LOG_MODULE_REGISTER(https_handler, 3);

char ota_url[OTA_URL_MAX_LEN];
char ota_hostname[CONFIG_DOWNLOAD_CLIENT_MAX_HOSTNAME_SIZE];
char ota_filename[CONFIG_DOWNLOAD_CLIENT_MAX_HOSTNAME_SIZE];

/* Forward declarations */
static const struct smf_state state[];

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(https_thread_channel, 5);

static struct s_object {
        /* This must be first */
        struct smf_ctx ctx;

        /* Last channel type that a message was received on */
        const struct zbus_channel *chan;

        /* Network status */
        enum network_status status;

        /* Sync HTTPS messages */
        int sync;
} s_obj;

struct https_client_t {
        /* connection status */
        bool connected;

        /** The server has closed the connection. */
        bool connection_close;

        const char *host;

        const char *path;

        struct {
                /** Header length */
                size_t hdr_len;
                /** Status code */
                unsigned long status_code;
                /** Whether the HTTP header for
                 * the current fragment has been processed.
                 */
                bool has_end;
        } header;

        struct {
                /** Socket descriptor. */
                int fd;
                /** Protocol for server. currently only supports TLS 1.2 */
                int proto;
                /** Socket type */
                int type;
                /** Port */
                uint16_t port;
		/* Destination address storage */
		struct sockaddr remote_addr;
        } sock;

        /* send buffer */
        char send_buf[SEND_BUF_SIZE];

        /* response buffer */
        char recv_buf[RECV_BUF_SIZE];
};

K_MSGQ_DEFINE(https_send_queue, SEND_BUF_SIZE, 10, 16);
static K_SEM_DEFINE(network_connected_sem, 0, 1);

static int httpPostLen;
static bool parse_https_rsp_for_ota_info = false;
static struct https_client_t https_client;
extern struct k_sem data_acq_start_sem;

/* A work queue is created to execute potentially blocking calls from.
 * This is done to avoid blocking for example the system work queue for extended
 * periods of time.
 */
struct k_work_delayable send_event_work;
struct k_work twin_report_work;
static K_THREAD_STACK_DEFINE(application_stack_area, APP_WORK_Q_STACK_SIZE);
static struct k_work_q application_work_q;

static void lte_conn_timer_handler(struct k_timer *dummy);
static void sync_https_data(struct k_timer *dummy);
static int https_client_connect(struct https_client_t *client);
static int https_client_disconnect(struct https_client_t *client);
static int https_client_reconnect(struct https_client_t *client);

K_TIMER_DEFINE(lte_conn_timer, lte_conn_timer_handler, NULL);
K_TIMER_DEFINE(https_syc_now, https_sync_data_handler, NULL);

void lte_conn_timer_handler(struct k_timer *dummy) {
        printk("Sensor LTE has been offline for atleast %d minutes. Rebooting...\n",
               LTE_NETWORK_CONN_TIMEOUT_MINUTES);
        lte_lc_power_off();
        sys_reboot(SYS_REBOOT_COLD);
}

/* Certificate for `example.com` */
static const char cert[] = {
#include "ngens.pem.inc"

    /* Null terminate certificate if running Mbed TLS on the application core.
     * Required by TLS credentials API.
     */
    IF_ENABLED(CONFIG_TLS_CREDENTIALS, (0x00))};

BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");

/* Provision certificate to modem */
int cert_provision(void) {
        int err;
        printk("Provisioning certificate\n");

#if CONFIG_MODEM_KEY_MGMT
        bool exists;
        int mismatch;

        /* It may be sufficient for you application to check whether the correct
         * certificate is provisioned with a given tag directly using modem_key_mgmt_cmp().
         * Here, for the sake of the completeness, we check that a certificate exists
         * before comparing it with what we expect it to be.
         */
        err = modem_key_mgmt_exists(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists);
        if (err) {
                printk("Failed to check for certificates err %d\n", err);
                return err;
        }

        if (exists) {
                mismatch = modem_key_mgmt_cmp(TLS_SEC_TAG,
                                              MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
                                              cert,
                                              sizeof(cert));
                if (!mismatch) {
                        printk("Certificate match\n");
                        return 0;
                }

                printk("Certificate mismatch\n");
                err = modem_key_mgmt_delete(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
                if (err) {
                        printk("Failed to delete existing certificate, err %d\n", err);
                }
        }

        printk("Provisioning certificate to the modem\n");

        /*  Provision certificate to the modem */
        err = modem_key_mgmt_write(TLS_SEC_TAG,
                                   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
                                   cert,
                                   sizeof(cert));
        if (err) {
                printk("Failed to provision certificate, err %d\n", err);
                return err;
        }
#else  /* CONFIG_MODEM_KEY_MGMT */
        err = tls_credential_add(TLS_SEC_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, cert, sizeof(cert));
        if (err == -EEXIST) {
                printk("CA certificate already exists, sec tag: %d\n", TLS_SEC_TAG);
        } else if (err < 0) {
                printk("Failed to register CA certificate: %d\n", err);
                return err;
        }
#endif /* !CONFIG_MODEM_KEY_MGMT */

        return 0;
}

/* Setup TLS options on a given socket */
int tls_setup(int fd) {
        int err;
        int verify;

        /* Security tag that we have provisioned the certificate with */
        const sec_tag_t tls_sec_tag[] = {
            TLS_SEC_TAG,
        };

        /* Set up TLS peer verification */
        enum {
                NONE     = 0,
                OPTIONAL = 1,
                REQUIRED = 2,
        };

        verify = REQUIRED;

        err    = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
        if (err) {
                printk("Failed to setup peer verification, err %d\n", errno);
                return err;
        }

        /* Associate the socket with the security tag
         * we have provisioned the certificate with.
         */
        err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag));
        if (err) {
                printk("Failed to setup TLS sec tag, err %d\n", errno);
                return err;
        }

        err = setsockopt(fd,
                         SOL_TLS,
                         TLS_HOSTNAME,
                         DATA_ACQ_HTTPS_HOSTNAME,
                         sizeof(DATA_ACQ_HTTPS_HOSTNAME) - 1);
        if (err) {
                printk("Failed to setup TLS hostname, err %d\n", errno);
                return err;
        }
        return 0;
}

static void lte_lc_handler(const struct lte_lc_evt *const evt) {
        /* CAUTION: Modem shall be gracefully deinitialized to prevent entering into Modem
         * Reset Loop Restriction for more detials see:
         * https://docs.nordicsemi.com/bundle/nwp_042/page/WP/nwp_042/intro.html
         */
        static bool connected;

        if (evt->type == LTE_LC_EVT_NW_REG_STATUS) {
                switch (evt->nw_reg_status) {
                case LTE_LC_NW_REG_NOT_REGISTERED:
                        /* This is the default case, no need to worry about it */
                        printk("LTE network not registered, not searching\n");
                        apply_ota_state(IDLE);
                        connected = false;
                        break;
                case LTE_LC_NW_REG_REGISTERED_HOME:
                        printk("LTE network registered home\n");
                        apply_ota_state(CONNECTED);
                        connected = true;
                        break;
                case LTE_LC_NW_REG_REGISTERED_ROAMING:
                        printk("LTE network registered roaming\n");
                        apply_ota_state(CONNECTED);
                        connected = true;
                        break;
                case LTE_LC_NW_REG_SEARCHING:
                        printk("LTE network searching.....\n");
                        apply_ota_state(IDLE);
                        connected = false;
                        break;
                case LTE_LC_NW_REG_REGISTRATION_DENIED:
                        /* we need to reboot */
                        printk("LTE network registration denied\n");
                        connected = false;
                        lte_lc_power_off();
                        sys_reboot(SYS_REBOOT_COLD);
                        break;
                case LTE_LC_NW_REG_UNKNOWN:
                        /* Disconnects and no-coverage scenarios fall under this case
                         * Network connection is lost, e.g. in elevator
                         */
                        printk("LTE network registration unknown\n");
                        apply_ota_state(IDLE);
                        connected = false;
                        break;
                case LTE_LC_NW_REG_UICC_FAIL:
                        /* Problem with SIM or eSIM - Hardware problem */
                        printk("LTE network UICC failure\n");
                        connected = false;
                        lte_lc_power_off();
                        sys_reboot(SYS_REBOOT_COLD);
                        break;
                default:
                        break;
                }
        } else {
                /* Don't care for now */
        }
        if (!connected) {
                /* Starting a timer that has already been started resets the timer
                 * We enter lte_lc_handler callback when there are network events.
                 * If there are no network events for LTE_NETWORK_CONN_TIMEOUT_MINUTES, and had been
                 * unconnected -- we need to perform a hard reboot. Else, modem is
                 * doing what it is supposed to be doing.
                 */
                k_timer_start(&lte_conn_timer,
                              K_MINUTES(LTE_NETWORK_CONN_TIMEOUT_MINUTES),
                              K_NO_WAIT);

                /* Notify HTTPS thread that LTE has been disconnected.
                 * Reset HTTPS state.
                 */
                enum network_status status = NETWORK_DISCONNECTED;
                err                        = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
                if (err) {
                        printk("zbus_chan_pub, error: %d", err);
                        SEND_FATAL_ERROR();
                }
        } else {
                k_timer_stop(&lte_conn_timer);
                k_sem_give(&network_connected_sem);

                /* Send HTTPS trigger to start syncing HTTPS messages */
                int val = 1;
                err     = zbus_chan_pub(&HTTPS_TRIGGER_CHAN, &val, K_SECONDS(5));
                if (err) {
                        printk("zbus_chan_pub, error: %d", err);
                        SEND_FATAL_ERROR();
                }
        }
}

static void send_event(struct k_work *work) {
        int err;
        char buf[SEND_BUF_SIZE];
        ssize_t len;

        /* Establish connection to the https end-point */
        err = https_client_connect(client);
        if (err != 0) {
                LOG_ERR("failed to sync: %d", err);
                return;
        }

        /* Keep syncing data until one of two happens:
         * - we completed syncing the send queue and there are no messages to send
         * - or, we lost LTE connection while syncing
         *
         * If the HTTPS connection is lost or connection-close is reported by the server
         * we do not need to stop, just reconnect again and keep sending
         */
        int num_messages = k_msgq_num_used_get(&https_send_queue);
        LOG_INF("Messages in HTTPS send message queue are :%d", num_messages);

        for (int i = 0; i < num_messages; i++) {
                err = k_msgq_get(&https_send_queue, &buf, K_NO_WAIT);
                if (err != 0) {
                        LOG_WRN("message retreive form HTTPS msgq failed..!\n");
                        continue;
                }
                LOG_INF("Sending event:%s", buf);

                /* SEND BUFFER */
                printk("%s", send_buf);
                httpPostLen = strlen(send_buf);
                off         = 0;
                do {
                        bytes = send(fd, &send_buf[off], httpPostLen - off, 0);
                        if (bytes < 0) {
                                printk("send() failed, err %d\n", errno);
                                goto clean_up;
                        }
                        off += bytes;
                } while (off < httpPostLen);

                printf("Sent %d bytes\n", off);

                /* RECEIVE INTO BUFFER */
                off = 0;
                do {
                        bytes = recv(fd, &recv_buf[off], RECV_BUF_SIZE - off, 0);
                        if (bytes < 0) {
                                printk("recv() failed, err %d\n", errno);
                                goto clean_up;
                        }
                        // printk("%.*s...",bytes,&recv_buf[off]);
                        off += bytes;
                } while (bytes != 0); /* peer closed connection */

                printk("Received %d bytes\n", off);

                /* Make sure recv_buf is NULL terminated (for safe use with strstr) */
                if (off < sizeof(recv_buf)) {
                        recv_buf[off] = '\0';
                } else {
                        recv_buf[sizeof(recv_buf) - 1] = '\0';
                }

                char *str_ptr = strstr(recv_buf, "fw_ver");

                if (str_ptr != NULL) {
                        parse_https_rsp_for_ota_info = true;
                }

                LOG_INF("https send OK");
        }

        /* disconnect from the client/ clean-up resources before exiting */
}

#define PORT_MAX_SIZE    5 /* 0xFFFF = 65535 */
#define PDN_ID_MAX_SIZE  2 /* 0..10 */

static int https_client_connect(struct https_client_t *client) {
        if (client->connected) {
                return -EALREADY;
        }

        int err;
        struct zsock_addrinfo *ai;
        struct zsock_addrinfo hints = {
            .ai_flags    = AI_NUMERICSERV | AI_PDNSERV,
            .ai_socktype = SOCK_STREAM,
            .ai_family   = AF_INET,
        };
	char service[PORT_MAX_SIZE + PDN_ID_MAX_SIZE + 2];

	/* "service" shall be formatted as follows: "port:pdn_id" */
	snprintf(service, sizeof(service), "%hu:%d", port, cid);

        printk("Looking up %s\n", client->host);
        err = zsock_getaddrinfo(client->host, service, &hints, &ai);
        

        inet_ntop(client->res->ai_family,
                  &((struct sockaddr_in *)(client->res->ai_addr))->sin_addr,
                  client->peer_addr,
                  INET6_ADDRSTRLEN);
        printk("Resolved %s (%s)\n", client->peer_addr, net_family2str(res->ai_family));

        if (IS_ENABLED(CONFIG_SAMPLE_TFM_MBEDTLS)) {
                client->fd = zsock_socket(client->res->ai_family,
                                          SOCK_STREAM | SOCK_NATIVE_TLS,
                                          IPPROTO_TLS_1_2);
        } else {
                client->fd = zsock_socket(client->res->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
        }
        if (client->fd == -1) {
                printk("Failed to open socket!\n");
                freeaddrinfo(res);
                return err;
        }

        /* Setup TLS socket options */
        err = tls_setup(client->fd);
        if (err) {
                client->connected = false;
                freeaddrinfo(res);
                return err;
        }

        printk("Connecting to %s:%d\n",
               client->host,
               ntohs(((struct sockaddr_in *)(client->res->ai_addr))->sin_port));
        err = connect(client->fd, client->res->ai_addr, client->res->ai_addrlen);
        if (err) {
                printk("connect() failed, err: %d\n", errno);
                client->connected = false;
                freeaddrinfo(res);
                return err;
        }

        client->connected = true;

        return 0;
}

static int https_client_disconnect(struct https_client_t *client) {
        /* Free resources and update flag */
        freeaddrinfo(client->res);
        (void)close(client->fd);

        client->connected = false;

        return 0;
}

static int https_client_reconnect(struct https_client_t *client) {
        int err;

        LOG_INF("Reconnecting....");

        err = https_client_disconnect(client);
        if (err) {
                LOG_ERR("disconnect failed, %d", err);
        }

        return https_client_connect(client);
}

static void sync_https_data(struct https_client_t *client) {
        parse_https_rsp_for_ota_info = false;
        int err;
        int bytes;
        size_t off;

clean_up:
        printk("Finished, closing socket.\n");

        freeaddrinfo(res);
        (void)close(fd);

        /* Runs only when recv buffer is loaded and there is a fw_ver header in the received
         * headers
         */
        if (parse_https_rsp_for_ota_info) {
                int idx          = 0;
                char ver_buf[32] = {0};

                str_ptr          = strstr(recv_buf, "fw_ver");
                str_ptr += strlen("fw_ver:");
                /* skip white spaces */
                while (*str_ptr == ' ') {
                        str_ptr++;
                }
                /* Load the version string into a local buffer */
                while (*str_ptr != '\r' && *str_ptr != '\n' && idx < sizeof(ver_buf) - 1) {
                        ver_buf[idx++] = *str_ptr++;
                }
                /* Make sure ver_buf is NULL terminated (for safe use with strstr) */
                ver_buf[idx] = '\0';

                printk("remote firmware version is: %s\r\n", ver_buf);

                /* Check if it didn't match our version and it is atleast as big to be a
                 * valid version string */
                if (strstr(ver_buf, APP_FW_VERSION) == NULL && strlen(ver_buf) > 4) {
                        printk("There is a firmware version mismatch\r\n");

                        /* get the link */
                        char *url_ptr = strstr(recv_buf, "fw_ota_url");
                        if (url_ptr == NULL) {
                                printk("didn't find fw_ota_url in reponse headers, aborting "
                                       "ota...\r\n");
                                goto clean_up;
                        }
                        url_ptr += strlen("fw_ota_url: ");

                        idx = 0;
                        memset(ota_url, 0, sizeof(ota_url));

                        while (*url_ptr && (*url_ptr != '\r' || *url_ptr != '\n') &&
                               idx < sizeof(ota_url) - 1) {
                                ota_url[idx++] = *url_ptr++;
                        }
                        ota_url[idx] = '\0';

                        /* Now check whether the url is valid before passing it to ota
                         * library */
                        const char *https_pattern = "https://";
                        char *parser_ptr          = strstr(ota_url, https_pattern);
                        if (parser_ptr == NULL) {
                                printk("ota error: https:// not found in url, currently only "
                                       "supports https");
                                goto clean_up;
                        }
                        parser_ptr += strlen(https_pattern);

                        memset(ota_hostname, '\0', sizeof(ota_hostname));
                        memset(ota_filename, '\0', sizeof(ota_filename));

                        idx = 0;
                        while (*parser_ptr && *parser_ptr != '/' &&
                               idx < CONFIG_DOWNLOAD_CLIENT_MAX_HOSTNAME_SIZE - 1) {
                                ota_hostname[idx++] = *parser_ptr++;
                        }
                        ota_hostname[idx] = '\0';

                        idx               = 0;
                        parser_ptr++;
                        while (*parser_ptr && *parser_ptr != '\0' && *parser_ptr != '\r' &&
                               *parser_ptr != '\n' &&
                               idx < CONFIG_DOWNLOAD_CLIENT_MAX_FILENAME_SIZE - 1) {
                                ota_filename[idx++] = *parser_ptr++;
                        }
                        ota_filename[idx] = '\0';

                        /* This triggers ota process */
                        apply_ota_state(UPDATE_DOWNLOAD);
                }
        }
}

/* ======== Zephyr State Machine framework handlers ============ */

/* Function executed when the https enters the disconnected state. */
static void disconnected_entry(void *o) {
        /* */
        struct s_object *user_object = o;

        /* Reschedule a connection attempt if we are connected to network and we enter the
         * disconnected state and there is no OTA in progress.
         */
        if (user_object->status == NETWORK_CONNECTED && !ota_session_in_progress) {
                k_work_reschedule_for_queue(&application_work_q, &connect_work, K_SECONDS(5));
        }
}

/* Function executed when the https is in the disconnected state. */
static void disconnected_run(void *o) {
        struct s_object *user_object = o;

        if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN)) {
                /* If NETWORK_DISCONNECTED is received after the HTTPS connection is closed,
                 * we cancel the connect work if it is onging.
                 */
                k_work_cancel_delayable(&connect_work);
        }

        if ((user_object->status == NETWORK_CONNECTED) && (user_object->chan == &NETWORK_CHAN) &&
            !ota_session_in_progress) {

                /* Wait for 5 seconds to ensure that the network stack is ready before
                 * attempting to connect to HTTPS. This delay is only needed when building
                 * for Wi-Fi.
                 */
                k_work_reschedule_for_queue(&application_work_q, &connect_work, K_SECONDS(5));
        }
}

/* Function executed when the https enters the connected state. */
static void connected_entry(void *o) {
        ARG_UNUSED(o);

        k_work_cancel_delayable(&connect_work);
        k_sem_give(&https_connected_sem);
        if (cert_updates_in_progress) {
                cert_updates_in_progress = false;
                printk("new certificates worked...!\n");
                /* overwrite the old with latest */
                new_cert_conn_retries = 0;
                overwrite_old_certs();
                save_current_sensor_state();
                k_msgq_put(&uart_msgq,
                           "HTTPS_CMD_CELL_NOTIFY_CERT_UPDATE_SUCCESS:success",
                           K_SECONDS(10));
        }
        subscribe();
}

/* Function executed when the https is in the connected state. */
static void connected_run(void *o) {
        struct s_object *user_object = o;

        if ((user_object->status == NETWORK_DISCONNECTED) && (user_object->chan == &NETWORK_CHAN)) {
                /* Explicitly disconnect the HTTPS transport when losing network
                 * connectivity. This is to cleanup any internal library state. The call to
                 * this function will cause on_https_disconnect() to be called.
                 */
                LOG_WRN("LTE Network loss has been reported, disconnecting HTTPS...");
                return;
        } else if ((user_object->status == NETWORK_CONNECTED) &&
                   (user_object->chan == &HTTPS_TRIGGER_CHAN)) {
                LOG_INF("LTE Network status is connected and received a trigger on "
                        "HTTPS_TRIGGER_CHAN, attempting sync...");
                k_work_reschedule_for_queue(&application_work_q, &send_event_work, K_MSEC(100));
        }
}

/* Function executed when the https exits the connected state. */
static void connected_exit(void *o) {
        ARG_UNUSED(o);

        LOG_INF("Disconnected from HTTPS end-point");
}

/* Construct state table */
static const struct smf_state state[] = {
    [HTTPS_DISCONNECTED] = SMF_CREATE_STATE(disconnected_entry, disconnected_run, NULL, NULL, NULL),
    [HTTPS_CONNECTED] =
        SMF_CREATE_STATE(connected_entry, connected_run, connected_exit, NULL, NULL),
};

int https_init(void) {
        int err;
        const struct zbus_channel *chan;
        enum network_status status;

        printk("HTTPS init started....\r\n");

        err = nrf_modem_lib_init();
        if (err < 0) {
                printk("Failed to initialize modem library!\n");
                return err;
        }

        /* OTA library certs for github.com */
        ota_update_init();

        err = cert_provision();
        if (err) {
                printk("Could not provision root CA to %d", TLS_SEC_TAG);
                return err;
        }

        printk("LTE Link Connecting ...\n");
        err = lte_lc_connect_async(lte_lc_handler);
        if (err) {
                printk("LTE link could not be established.");
                return err;
        }

        https_client.connected          = false;
        https_client.connection_close   = false;
        https_client.header.hdr_len     = 0;
        https_client.header.status_code = 0;
        https_client.header.has_end     = false;

        https_client.host               = DATA_ACQ_HTTPS_HOSTNAME;
        https_client.path               = DATA_ACQ_HTTPS_TARGET;

        https_client.sock.fd            = -1;
        https_client.sock.port          = atoi(DATA_ACQ_HTTPS_PORT);
        https_client.sock.proto         = IPPROTO_TLS_1_2;
        https_client.sock.type          = SOCK_STREAM;

        k_sem_take(&network_connected_sem, K_FOREVER);
        k_sem_give(&data_acq_start_sem);

        return 0;
}

void https_thread_entry(void *a, void *b, void *c) {
        printk("HTTPS thread starting...\n");

        k_work_init_delayable(&send_event_work, send_event);
        k_work_queue_start(&application_work_q,
                           application_stack_area,
                           K_THREAD_STACK_SIZEOF(application_stack_area),
                           K_HIGHEST_APPLICATION_THREAD_PRIO,
                           NULL);

        /* Set initial state */
        smf_set_initial(SMF_CTX(&s_obj), &state[HTTPS_DISCONNECTED]);

        while (!zbus_sub_wait(&https_thread_channel, &chan, K_FOREVER)) {

                s_obj.chan = chan;

                if (&NETWORK_CHAN == chan) {
                        /* This channel is used to manage HTTPS connection & its states */
                        err = zbus_chan_read(&NETWORK_CHAN, &status, K_SECONDS(1));
                        if (err) {
                                printk("zbus_chan_read, error: %d", err);
                                SEND_FATAL_ERROR();
                                return;
                        }

                        s_obj.status = status;

                        err          = smf_run_state(SMF_CTX(&s_obj));
                        if (err) {
                                printk("smf_run_state, error: %d", err);
                                SEND_FATAL_ERROR();
                                return;
                        }
                }

                if (&HTTPS_TRIGGER_CHAN == chan) {
                        /* This channel is used to trigger a sync via HTTPS */
                        err = zbus_chan_read(&HTTPS_TRIGGER_CHAN, &val, K_SECONDS(1));
                        if (err) {
                                printk("zbus_chan_read, error: %d", err);
                                SEND_FATAL_ERROR();
                                return;
                        }

                        s_obj.sync = val;

                        err        = smf_run_state(SMF_CTX(&s_obj));
                        if (err) {
                                printk("smf_run_state, error: %d", err);
                                SEND_FATAL_ERROR();
                                return;
                        }
                }
        }
}
