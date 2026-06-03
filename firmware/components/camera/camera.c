#include "camera.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "camera";

#define NVS_NS "m5cam"

static SemaphoreHandle_t s_camera_mutex;

/* OV3660 GPIO map for M5 Timer Camera X.
 * fb_count is overridden per call in camera_init() (one-shot vs. streaming). */
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
    .frame_size   = FRAMESIZE_UXGA,
    .jpeg_quality = 12,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

static bool framesize_from_name(const char *name, framesize_t *framesize)
{
    static const struct {
        const char *name;
        framesize_t framesize;
    } values[] = {
        {"VGA", FRAMESIZE_VGA},
        {"SVGA", FRAMESIZE_SVGA},
        {"XGA", FRAMESIZE_XGA},
        {"SXGA", FRAMESIZE_SXGA},
        {"UXGA", FRAMESIZE_UXGA},
        {"QXGA", FRAMESIZE_QXGA},
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        if (strcmp(values[i].name, name) == 0) {
            *framesize = values[i].framesize;
            return true;
        }
    }
    return false;
}

static bool config_valid(const camera_image_config_t *config)
{
    framesize_t framesize;
    return framesize_from_name(config->framesize, &framesize)
        && config->quality >= 0 && config->quality <= 63
        && config->brightness >= -3 && config->brightness <= 3
        && config->contrast >= -3 && config->contrast <= 3
        && config->saturation >= -4 && config->saturation <= 4
        && config->sharpness >= -3 && config->sharpness <= 3;
}

void camera_image_config_defaults(camera_image_config_t *config)
{
    *config = (camera_image_config_t) {
        .framesize = "UXGA",
        .quality = 12,
        .brightness = 0,
        .contrast = 0,
        .saturation = 0,
        .sharpness = 0,
        .hmirror = true,
        .vflip = true,
    };
}

esp_err_t camera_image_config_load(camera_image_config_t *config)
{
    camera_image_config_defaults(config);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return ESP_OK;

    size_t size = sizeof(config->framesize);
    int32_t value;
    nvs_get_str(h, "framesize", config->framesize, &size);
    if (nvs_get_i32(h, "quality", &value) == ESP_OK) config->quality = value;
    if (nvs_get_i32(h, "brightness", &value) == ESP_OK) config->brightness = value;
    if (nvs_get_i32(h, "contrast", &value) == ESP_OK) config->contrast = value;
    if (nvs_get_i32(h, "saturation", &value) == ESP_OK) config->saturation = value;
    if (nvs_get_i32(h, "sharpness", &value) == ESP_OK) config->sharpness = value;
    if (nvs_get_i32(h, "hmirror", &value) == ESP_OK) config->hmirror = value != 0;
    if (nvs_get_i32(h, "vflip", &value) == ESP_OK) config->vflip = value != 0;
    nvs_close(h);

    if (!config_valid(config)) {
        ESP_LOGW(TAG, "invalid saved image config, using defaults");
        camera_image_config_defaults(config);
    }
    return ESP_OK;
}

static esp_err_t config_persist(const camera_image_config_t *config)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "framesize", config->framesize);
    if (err == ESP_OK) err = nvs_set_i32(h, "quality", config->quality);
    if (err == ESP_OK) err = nvs_set_i32(h, "brightness", config->brightness);
    if (err == ESP_OK) err = nvs_set_i32(h, "contrast", config->contrast);
    if (err == ESP_OK) err = nvs_set_i32(h, "saturation", config->saturation);
    if (err == ESP_OK) err = nvs_set_i32(h, "sharpness", config->sharpness);
    if (err == ESP_OK) err = nvs_set_i32(h, "hmirror", config->hmirror);
    if (err == ESP_OK) err = nvs_set_i32(h, "vflip", config->vflip);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t config_apply_locked(const camera_image_config_t *config)
{
    framesize_t framesize;
    if (!config_valid(config) || !framesize_from_name(config->framesize, &framesize)) {
        return ESP_ERR_INVALID_ARG;
    }
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return ESP_FAIL;
    if (s->set_framesize(s, framesize)
        || s->set_quality(s, config->quality)
        || s->set_brightness(s, config->brightness)
        || s->set_contrast(s, config->contrast)
        || s->set_saturation(s, config->saturation)
        || s->set_sharpness(s, config->sharpness)
        || s->set_hmirror(s, config->hmirror)
        || s->set_vflip(s, config->vflip)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t camera_image_config_apply(const camera_image_config_t *config, bool persist)
{
    if (!s_camera_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);
    esp_err_t err = config_apply_locked(config);
    if (err == ESP_OK && persist) err = config_persist(config);
    xSemaphoreGive(s_camera_mutex);
    return err;
}

esp_err_t camera_image_config_get(camera_image_config_t *config)
{
    if (!s_camera_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        xSemaphoreGive(s_camera_mutex);
        return ESP_FAIL;
    }
    camera_image_config_defaults(config);
    static const char *names[] = {
        [FRAMESIZE_VGA] = "VGA", [FRAMESIZE_SVGA] = "SVGA",
        [FRAMESIZE_XGA] = "XGA", [FRAMESIZE_SXGA] = "SXGA",
        [FRAMESIZE_UXGA] = "UXGA", [FRAMESIZE_QXGA] = "QXGA",
    };
    if (s->status.framesize < sizeof(names) / sizeof(names[0])
        && names[s->status.framesize]) {
        strlcpy(config->framesize, names[s->status.framesize], sizeof(config->framesize));
    }
    config->quality = s->status.quality;
    config->brightness = s->status.brightness;
    config->contrast = s->status.contrast;
    config->saturation = s->status.saturation;
    config->sharpness = s->status.sharpness;
    config->hmirror = s->status.hmirror;
    config->vflip = s->status.vflip;
    xSemaphoreGive(s_camera_mutex);
    return ESP_OK;
}

esp_err_t camera_init(uint8_t fb_count)
{
    s_camera_mutex = xSemaphoreCreateMutex();
    if (!s_camera_mutex) return ESP_ERR_NO_MEM;

    camera_config_t hw_cfg = CAMERA_CONFIG;
    hw_cfg.fb_count = fb_count;
    esp_err_t err = esp_camera_init(&hw_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
        return err;
    }
    camera_image_config_t config;
    camera_image_config_load(&config);
    esp_err_t cfg_err = camera_image_config_apply(&config, false);
    if (cfg_err != ESP_OK) return cfg_err;

    /* OV3660 needs one frame to stabilise after waking from deep sleep.
     * Discarded AFTER config apply so any framesize change takes effect first;
     * without this the first real capture sometimes returns NULL. */
    camera_fb_t *warmup = esp_camera_fb_get();
    if (warmup) {
        esp_camera_fb_return(warmup);
        ESP_LOGI(TAG, "sensor warmed up");
    } else {
        ESP_LOGW(TAG, "warmup frame not available — sensor may be slow");
    }
    return ESP_OK;
}

static esp_err_t capture_locked(uint8_t **buf, size_t *len)
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

esp_err_t camera_capture(uint8_t **buf, size_t *len)
{
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);
    esp_err_t err = capture_locked(buf, len);
    xSemaphoreGive(s_camera_mutex);
    return err;
}

esp_err_t camera_preview_capture(const camera_image_config_t *config,
                                 uint8_t **buf, size_t *len)
{
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);
    esp_err_t err = config_apply_locked(config);
    if (err == ESP_OK) err = capture_locked(buf, len);
    xSemaphoreGive(s_camera_mutex);
    return err;
}

void camera_release(uint8_t *buf)
{
    free(buf);
}
