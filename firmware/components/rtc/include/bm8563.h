#pragma once
#include "esp_err.h"
#include <time.h>

esp_err_t bm8563_init(void);

/* Read current time from BM8563. */
esp_err_t bm8563_get_time(struct tm *t);

/* Set a deep-sleep wake alarm N minutes from now, then enter deep sleep.
 * Releases Battery Hold (GPIO33 LOW) before sleeping. */
esp_err_t bm8563_set_wake_alarm(int minutes_from_now);
