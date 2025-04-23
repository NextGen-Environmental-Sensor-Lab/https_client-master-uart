#ifndef HTTPS_HANDLER_H
#define HTTPS_HANDLER_H

#define HTTPS_POST_REQUEST                                              	     \
        "POST %s HTTP/1.1\r\n"                                                       \
        "Host: %s:%s\r\n"                                                            \
        "Connection: close\r\n"                                                      \
        "Authorization: Basic bmdlbnNhZG1pbjpONjNuNWFkbTFu\r\n"                      \
        "Content-Type: application/json\r\n"                                         \
        "Content-Length: %d\r\n"                                                     \
        "\r\n"                                                                       \
        "%s\r\n\r\n"

#define RECV_BUF_SIZE 2048
#define SEND_BUF_SIZE 2048
#define TLS_SEC_TAG 2334
#define DATA_BUF_SIZE 2048

int https_init(void);

#endif