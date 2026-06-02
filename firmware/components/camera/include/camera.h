#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

esp_err_t camera_init(void);

/* Capture a JPEG. Caller must call camera_release() when done. */
esp_err_t camera_capture(uint8_t **buf, size_t *len);

void camera_release(uint8_t *buf);
