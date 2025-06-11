#include "watchdog.h"

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/printk.h>

extern volatile bool uart_handler_alive;
extern volatile bool https_handler_alive;

#define WDT_FEED_INTERVAL (CONFIG_WATCHDOG_APPLICATION_TIMEOUT_SEC * 1000)

#ifndef WDT_MAX_WINDOW 
#define WDT_MAX_WINDOW  WDT_FEED_INTERVAL + 1000U
#endif

#ifndef WDT_OPT
#define WDT_OPT WDT_OPT_PAUSE_HALTED_BY_DBG
#endif

const struct device *const wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));
int wdt_channel_id;

void watchdog_callback(const struct device *dev, int channel_id) {
    /* This will be called if the watchdog is not fed in time */
    printk("Watchdog timeout occurred! System will reset.\n");
}

void watchdog_feeder_thread(void *arg1, void *arg2, void *arg3) {
	int err;

	if (!device_is_ready(wdt)) {
		printk("%s: device not ready.\n", wdt->name);
		return;
	}

	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Expire watchdog after max window */
		.window.min = 0,
		.window.max = WDT_MAX_WINDOW,
		.callback = watchdog_callback,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id == -ENOTSUP) {
		printk("Callback support rejected, continuing anyway\n");
		wdt_config.callback = NULL;
		wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	}
	if (wdt_channel_id < 0) {
		printk("Watchdog install error: %d\n", wdt_channel_id);
		return;
	}

	err = wdt_setup(wdt, WDT_OPT);
	if (err < 0) {
		printk("Watchdog setup error\n");
		return;
	} else {
		printk("watchdog setup success!\n");
	}

	while(1) {
		k_sleep(K_MSEC(WDT_FEED_INTERVAL));
		
		if (ALL_THREADS_ALIVE) {
			uart_handler_alive = https_handler_alive = false;
			/* Feed the dog */
			wdt_feed(wdt, wdt_channel_id);
		}
	}
}