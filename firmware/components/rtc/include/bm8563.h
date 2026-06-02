#pragma once
#include "esp_err.h"
#include <time.h>

esp_err_t bm8563_init(void);

/* Read current time from BM8563. */
esp_err_t bm8563_get_time(struct tm *t);

/* Program BM8563 and ESP32 wake timers N minutes from now, then release Battery
 * Hold (GPIO33 LOW). BM8563 powers the board back on after a battery sleep;
 * ESP32 deep sleep covers the USB-powered case. */
esp_err_t bm8563_set_wake_alarm(int minutes_from_now);

/* Power off indefinitely without scheduling a wake timer. */
esp_err_t bm8563_power_off(void);
