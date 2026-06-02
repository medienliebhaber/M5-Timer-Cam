#include "camera.h"
#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

/* OV3660 GPIO map for M5 Timer Camera X */
static const camera_config_t CAMERA_CONFIG = {
    .pin_pwdn     = -1,
    .pin_reset    = 15,
    .pin_xclk     = 27,
    .pin_sccb_sda = 25,
    .pin_sccb_scl = 23,
    .pin_d7       = 19,
    .pin_d6       = 36,
    .pin_d5       = 18,
    .pin_d4       = 39,
    .pin_d3       = 5,
    .pin_d2       = 34,
    .pin_d1       = 35,
    .pin_d0       = 32,
    .pin_vsync    = 22,
    .pin_href     = 26,
    .pin_pclk     = 21,

    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_UXGA,   /* 1600×1200 */
    .jpeg_quality = 12,               /* 0-63, lower = better */
    .fb_count     = 2,                /* double-buffer in PSRAM */
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

esp_err_t camera_init(void)
{
    esp_err_t err = esp_camera_init(&CAMERA_CONFIG);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t camera_capture(uint8_t **buf, size_t *len)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "failed to get framebuffer");
        return ESP_FAIL;
    }

    *buf = malloc(fb->len);
    if (!*buf) {
        esp_camera_fb_return(fb);
        return ESP_ERR_NO_MEM;
    }

    memcpy(*buf, fb->buf, fb->len);
    *len = fb->len;
    esp_camera_fb_return(fb);

    ESP_LOGI(TAG, "captured %zu bytes", *len);
    return ESP_OK;
}

void camera_release(uint8_t *buf)
{
    free(buf);
}
