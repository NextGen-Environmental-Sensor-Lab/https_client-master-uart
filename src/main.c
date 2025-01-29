/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/tls_credentials.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#if defined(CONFIG_POSIX_API)
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/sys/socket.h>
#endif

#if CONFIG_MODEM_KEY_MGMT
#include <modem/modem_key_mgmt.h>
#endif

#define HTTPS_HOSTNAME "script.google.com"
#define HTTPS_PORT "443"
#define HTTPS_TARGET "/macros/s/AKfycbyYJDElhjQQdPbhONXswdhvUNnWmd6RTAB9eoQavpZkyNH1cietC8S0KZ7HGoI7ytCVtA/exec"
#define HTTP_GET                                  \
	"GET " HTTPS_TARGET " HTTP/1.1\r\n"           \
	"Host: " HTTPS_HOSTNAME ":" HTTPS_PORT "\r\n" \
	"Connection: close\r\n\r\n"
#define HTTP_POST                                 \
	"POST " HTTPS_TARGET " HTTP/1.1\r\n"          \
	"Host: " HTTPS_HOSTNAME ":" HTTPS_PORT "\r\n" \
	"Content-Type: application/json\r\n"          \
	"Content-Length: " // append contentstring length, \r\n and contentstring
#define HTTP_POST_COMMANDS "{\"command\":\"appendRow\",\"sheet_name\":\"Sheet1\",\"values\":\""
#define HTTP_POST_COMMANDS_END "\"}"

// #define HTTP_HEAD		\
				// "HEAD / HTTP/1.1\r\n"	\
				// "Host: " CONFIG_HTTPS_HOSTNAME ":" HTTPS_PORT "\r\n"		\
				// "Connection: close\r\n\r\n"

#define HTTP_GET_LEN (sizeof(HTTP_GET) - 1)
#define HTTP_GET_END "\r\n\r\n"

#define RECV_BUF_SIZE 2048
#define SEND_BUF_SIZE 2048
#define TLS_SEC_TAG 123
#define DATA_BUF_SIZE 2048

static char send_buf[SEND_BUF_SIZE];
static char recv_buf[RECV_BUF_SIZE];
static int httpPostLen;
static char data_buf[DATA_BUF_SIZE];

static K_SEM_DEFINE(network_connected_sem, 0, 1);
/* Certificate for `example.com` */
static const char cert[] = {
#include "DigiCertGlobalG2.pem.inc"

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
					 HTTPS_HOSTNAME,
					 sizeof(HTTPS_HOSTNAME) - 1);
	if (err)
	{
		printk("Failed to setup TLS hostname, err %d\n", errno);
		return err;
	}
	return 0;
}

static void lte_lc_handler(const struct lte_lc_evt *const evt)
{
	static bool connected;

	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
			(evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
		{
			if (!connected)
			{
				break;
			}

			printk("LTE network is disconnected.\n");
			connected = false;
			break;
		}
		connected = true;
		k_sem_give(&network_connected_sem);
		break;
	default:
		break;
	}
}

static void build_http_request(void)
{
	// strcpy(send_buf, HTTP_GET);

	strcpy(send_buf, HTTP_POST);

	// function to put data string in data_buf

	int httpPayloadLen = strlen(HTTP_POST_COMMANDS)+ strlen(data_buf) + strlen(HTTP_POST_COMMANDS_END);

	char temp[16];
	sprintf(temp, "%u", httpPayloadLen); // converts the number to string
	strcat(send_buf, temp);
	strcat(send_buf, "\r\nConnection: close\r\n\r\n");
	strcat(send_buf, HTTP_POST_COMMANDS);
	strcat(send_buf,data_buf);
	strcat(send_buf,HTTP_POST_COMMANDS_END);

	//char httpPayload[] = "{\"command\":\"appendRow\",\"sheet_name\":\"Sheet1\",\"values\":\"1,2,3,4,nrf9160\"}";

	httpPostLen = strlen(send_buf);

	printf("send_buf size: %u\nsend_buf: \n%s\n", httpPostLen, send_buf);
}

static void send_http_request(void)
{
	int err;
	int fd;
	char *p;
	int bytes;
	size_t off;
	struct addrinfo *res;
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV, /* Let getaddrinfo() set port */
		.ai_socktype = SOCK_STREAM,
	};
	char peer_addr[INET6_ADDRSTRLEN];

	printk("Looking up %s\n", HTTPS_HOSTNAME);

	err = getaddrinfo(HTTPS_HOSTNAME, HTTPS_PORT, &hints, &res);
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

	printk("Connecting to %s:%d\n", HTTPS_HOSTNAME,
		   ntohs(((struct sockaddr_in *)(res->ai_addr))->sin_port));
	err = connect(fd, res->ai_addr, res->ai_addrlen);
	if (err)
	{
		printk("connect() failed, err: %d\n", errno);
		goto clean_up;
	}

	/* SEND BUFFER */
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
		//printf("%.*s...",bytes,&recv_buf[off]);
		off += bytes;
	} while (bytes != 0 ); /* peer closed connection */

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

int main(void)
{
	int err;
	strcat(data_buf,"11,22,33,44,55"); //dummy data

	printk("HTTPS client sample started\n\r");

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
	// printk("after net sem\n");

	build_http_request();

	while (1)
	{
		send_http_request();
		k_sleep(K_SECONDS(60));
	}

	return 0;
}
