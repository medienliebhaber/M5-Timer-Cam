#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    char framesize[8];
    int quality;
    int brightness;
    int contrast;
    int saturation;
    int sharpness;
    bool hmirror;
    bool vflip;
} camera_image_config_t;

/* Framebuffer counts: one-shot battery capture vs. awake streaming. */
#define FB_COUNT_ONESHOT 1
#define FB_COUNT_STREAM  2

esp_err_t camera_init(uint8_t fb_count);

/* Capture a JPEG. Caller must call camera_release() when done. */
esp_err_t camera_capture(uint8_t **buf, size_t *len);

void camera_image_config_defaults(camera_image_config_t *config);
esp_err_t camera_image_config_load(camera_image_config_t *config);
esp_err_t camera_image_config_get(camera_image_config_t *config);
esp_err_t camera_image_config_apply(const camera_image_config_t *config, bool persist);
esp_err_t camera_preview_capture(const camera_image_config_t *config,
                                 uint8_t **buf, size_t *len);

void camera_release(uint8_t *buf);
