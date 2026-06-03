#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t http_server_start(void);
void      http_server_stop(void);

/* Camera config — reads from NVS, falls back to compile-time defaults */
int  cam_config_get_interval(void);
