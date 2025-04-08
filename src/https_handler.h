#ifndef HTTPS_HANDLER_H
#define HTTPS_HANDLER_H

#define HTTPS_HOSTNAME  "script.google.com"
#define HTTPS_PORT      "443"
#define HTTPS_TARGET    "/macros/s/AKfycbyYJDElhjQQdPbhONXswdhvUNnWmd6RTAB9eoQavpZkyNH1cietC8S0KZ7HGoI7ytCVtA/exec"

#define HTTP_POST_MESSAGE                           \
        "{"                                         \
            "\"command\":\"appendRow\","            \
            "\"sheet_name\":\"Sheet1\","            \
            "\"values\":\"%s,%d\""                  \
        "}"

#define HTTPS_POST_REGULAR_UPLOAD                                              	     \
        "POST %s HTTP/1.1\r\n"                                                       \
        "Host: %s:%s\r\n"                                                            \
        "Connection: close\r\n"                                                      \
        "Content-Type: application/json\r\n"                                         \
        "Content-Length: %d\r\n"                                                     \
        "\r\n"                                                                       \
        "%s\r\n\r\n"

#define RECV_BUF_SIZE 2048
#define SEND_BUF_SIZE 2048
#define TLS_SEC_TAG 123
#define DATA_BUF_SIZE 2048

int https_init(void);

#endif