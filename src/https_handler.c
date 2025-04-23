#include "https_handler.h"
#include "data_acq.h"

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_POSIX_API)
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/sys/socket.h>
#endif

#if CONFIG_MODEM_KEY_MGMT
#include <modem/modem_key_mgmt.h>
#endif

#define LTE_NETWORK_CONN_TIMEOUT_MINUTES 5 /* change to 30 minutes in the field */

LOG_MODULE_REGISTER(https_handler, 3);

void my_timer_handler(struct k_timer *dummy);

K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);

void my_timer_handler(struct k_timer *dummy)
{
	printk("Sensor LTE has been offline for atleast %d minutes. Rebooting...\n", LTE_NETWORK_CONN_TIMEOUT_MINUTES);
	lte_lc_power_off();
	sys_reboot(SYS_REBOOT_COLD);
}

K_MSGQ_DEFINE(https_send_queue, SEND_BUF_SIZE, 10, 16);

static char send_buf[SEND_BUF_SIZE];
static char recv_buf[RECV_BUF_SIZE];
static int httpPostLen;

static K_SEM_DEFINE(network_connected_sem, 0, 1);

extern struct k_sem data_acq_start_sem;
/* Certificate for `example.com` */
static const char cert[] = {
#include "ngens.pem.inc"

	/* Null terminate certificate if running Mbed TLS on the application core.
	 * Required by TLS credentials API.
	 */
	IF_ENABLED(CONFIG_TLS_CREDENTIALS, (0x00))};

BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");

/* Provision certificate to modem */
int cert_provision(void)
{
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
	if (err)
	{
		printk("Failed to check for certificates err %d\n", err);
		return err;
	}

	if (exists)
	{
		mismatch = modem_key_mgmt_cmp(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
									  sizeof(cert));
		if (!mismatch)
		{
			printk("Certificate match\n");
			return 0;
		}

		printk("Certificate mismatch\n");
		err = modem_key_mgmt_delete(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		if (err)
		{
			printk("Failed to delete existing certificate, err %d\n", err);
		}
	}

	printk("Provisioning certificate to the modem\n");

	/*  Provision certificate to the modem */
	err = modem_key_mgmt_write(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
							   sizeof(cert));
	if (err)
	{
		printk("Failed to provision certificate, err %d\n", err);
		return err;
	}
#else  /* CONFIG_MODEM_KEY_MGMT */
	err = tls_credential_add(TLS_SEC_TAG,
							 TLS_CREDENTIAL_CA_CERTIFICATE,
							 cert,
							 sizeof(cert));
	if (err == -EEXIST)
	{
		printk("CA certificate already exists, sec tag: %d\n", TLS_SEC_TAG);
	}
	else if (err < 0)
	{
		printk("Failed to register CA certificate: %d\n", err);
		return err;
	}
#endif /* !CONFIG_MODEM_KEY_MGMT */

	return 0;
}

/* Setup TLS options on a given socket */
int tls_setup(int fd)
{
	int err;
	int verify;

	/* Security tag that we have provisioned the certificate with */
	const sec_tag_t tls_sec_tag[] = {
		TLS_SEC_TAG,
	};

	/* Set up TLS peer verification */
	enum
	{
		NONE = 0,
		OPTIONAL = 1,
		REQUIRED = 2,
	};

	verify = REQUIRED;

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err)
	{
		printk("Failed to setup peer verification, err %d\n", errno);
		return err;
	}

	/* Associate the socket with the security tag
	 * we have provisioned the certificate with.
	 */
	err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag));
	if (err)
	{
		printk("Failed to setup TLS sec tag, err %d\n", errno);
		return err;
	}

	err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME,
					 DATA_ACQ_HTTPS_HOSTNAME,
					 sizeof(DATA_ACQ_HTTPS_HOSTNAME) - 1);
	if (err)
	{
		printk("Failed to setup TLS hostname, err %d\n", errno);
		return err;
	}
	return 0;
}

static void lte_lc_handler(const struct lte_lc_evt *const evt) {
	/* CAUTION: Modem shall be gracefully deinitialized to prevent entering into Modem Reset Loop Restriction 
	 * 	for more detials see: https://docs.nordicsemi.com/bundle/nwp_042/page/WP/nwp_042/intro.html
	 */
	static bool connected;

	if (evt->type == LTE_LC_EVT_NW_REG_STATUS) {
		switch (evt->nw_reg_status) {
			case LTE_LC_NW_REG_NOT_REGISTERED:	
			/* This is the default case, no need to worry about it */
			printk("LTE network not registered, not searching\n");
			connected = false;
			break;			
		case LTE_LC_NW_REG_REGISTERED_HOME:
			printk("LTE network registered home\n");
			connected = true;
			break;
		case LTE_LC_NW_REG_REGISTERED_ROAMING:
			printk("LTE network registered roaming\n");
			connected = true;
			break;
		case LTE_LC_NW_REG_SEARCHING:
			printk("LTE network searching.....\n");
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
		/* Don't care */
	}
	if (!connected)
	{
		/* Starting a timer that has already been started resets the timer
		 * We enter lte_lc_handler callback when there are network events. 
		 * If there are no network events for 5 minutes, and had been 
		 * unconnected -- we need to perform a hard reboot. Else, modem is 
		 * doing what it is supposed to be doing.
		 */
		k_timer_start(&my_timer, K_MINUTES(LTE_NETWORK_CONN_TIMEOUT_MINUTES), K_NO_WAIT);
	}
	else
	{
		k_timer_stop(&my_timer);
		k_sem_give(&network_connected_sem);
	}
}

static void send_http_request(void)
{
	int err;
	int fd;
	int bytes;
	size_t off;
	struct addrinfo *res;
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV, /* Let getaddrinfo() set port */
		.ai_socktype = SOCK_STREAM,
	};
	char peer_addr[INET6_ADDRSTRLEN];

	printk("Looking up %s\n", DATA_ACQ_HTTPS_HOSTNAME);

	err = getaddrinfo(DATA_ACQ_HTTPS_HOSTNAME, DATA_ACQ_HTTPS_PORT, &hints, &res);
	if (err)
	{
		printk("getaddrinfo() failed, err %d\n", errno);
		return;
	}

	inet_ntop(res->ai_family, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, peer_addr, INET6_ADDRSTRLEN);
	printk("Resolved %s (%s)\n", peer_addr, net_family2str(res->ai_family));

	if (IS_ENABLED(CONFIG_SAMPLE_TFM_MBEDTLS))
	{
		fd = socket(res->ai_family, SOCK_STREAM | SOCK_NATIVE_TLS, IPPROTO_TLS_1_2);
	}
	else
	{
		fd = socket(res->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
	}
	if (fd == -1)
	{
		printk("Failed to open socket!\n");
		goto clean_up;
	}

	/* Setup TLS socket options */
	err = tls_setup(fd);
	if (err)
	{
		goto clean_up;
	}

	printk("Connecting to %s:%d\n", DATA_ACQ_HTTPS_HOSTNAME,
		   ntohs(((struct sockaddr_in *)(res->ai_addr))->sin_port));
	err = connect(fd, res->ai_addr, res->ai_addrlen);
	if (err)
	{
		printk("connect() failed, err: %d\n", errno);
		goto clean_up;
	}

	/* SEND BUFFER */
	printk("%s", send_buf);
	httpPostLen = strlen(send_buf);
	off = 0;
	do
	{
		bytes = send(fd, &send_buf[off], httpPostLen - off, 0);
		if (bytes < 0)
		{
			printk("send() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (off < httpPostLen);

	printf("Sent %d bytes\n", off);

	/* RECEIVE INTO BUFFER */
	off = 0;
	do
	{
		bytes = recv(fd, &recv_buf[off], RECV_BUF_SIZE - off, 0);
		if (bytes < 0)
		{
			printk("recv() failed, err %d\n", errno);
			goto clean_up;
		}
		// printf("%.*s...",bytes,&recv_buf[off]);
		off += bytes;
	} while (bytes != 0); /* peer closed connection */

	// printk("Received %d bytes\n", off);
	printf("Received %d bytes\n", off);

	/* Make sure recv_buf is NULL terminated (for safe use with strstr) */
	if (off < sizeof(recv_buf))
	{
		recv_buf[off] = '\0';
	}
	else
	{
		recv_buf[sizeof(recv_buf) - 1] = '\0';
	}

	/* Print HTTP response */
	/* p = strstr(recv_buf, "\r\n");
	if (p) {
		off = p - recv_buf;
		recv_buf[off + 1] = '\0';
		printk("\n>\t %s\n\n", recv_buf);
	}
	*/
	// printk("\n\n%s\n\n", recv_buf);
	printf(">\n>\n%s>\n>\n", recv_buf);

	// printk("Finished, closing socket.\n");
	printf("Finished, closing socket.\n");

clean_up:
	freeaddrinfo(res);
	(void)close(fd);
}

int https_init(void)
{
	int err;

	printk("HTTPS init started....\r\n");

	err = nrf_modem_lib_init();
	if (err < 0)
	{
		printk("Failed to initialize modem library!\n");
		return err;
	}

	err = cert_provision();
	if (err)
	{
		printk("Could not provision root CA to %d", TLS_SEC_TAG);
		return err;
	}

	printk("LTE Link Connecting ...\n");
	err = lte_lc_connect_async(lte_lc_handler);
	if (err)
	{
		printk("LTE link could not be established.");
		return err;
	}

	k_sem_take(&network_connected_sem, K_FOREVER);
	k_sem_give(&data_acq_start_sem);

	return 0;
}

void https_thread_entry(void *a, void *b, void *c)
{
	printk("HTTPS thread starting...\n");
	
	while (1)
	{
		/* Wait forever until there is a message in the https_send_queue */
		if (k_msgq_get(&https_send_queue, &send_buf, K_FOREVER) == 0)
		{
			send_http_request();
		}
		k_sleep(K_SECONDS(1));
	}
}