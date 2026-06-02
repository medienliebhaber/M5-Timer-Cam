#pragma once
#include "esp_err.h"
#include <stdbool.h>

/* Connect to the configured SSID. Blocks until connected or max retries. */
esp_err_t wifi_connect(void);

void wifi_disconnect(void);

bool wifi_is_connected(void);
