#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <zephyr/device.h>

int uart_init(const struct device *dev);
void print_uart(const struct device *dev, char *data);

#endif