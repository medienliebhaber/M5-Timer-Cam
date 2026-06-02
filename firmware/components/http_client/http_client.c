#include "http_client.h"
#include "bm8563.h"
#include "camera.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "http_client";

#define NVS_NS      "m5cam"
#define NVS_KEY_INT "interval"
#define NVS_KEY_SLP "sleep_en"

static void apply_config_header(esp_http_client_handle_t client,
                                 const char *header, const char *nvs_key)
{
    char *val = NULL;
    esp_http_client_get_header(client, header, &val);
    if (!val || *val == '\0') return;

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    if (strcmp(nvs_key, NVS_KEY_INT) == 0) {
        int v = atoi(val);
        if (v >= 1 && v <= 1440) {
            nvs_set_i32(h, nvs_key, (int32_t)v);
            ESP_LOGI(TAG, "config: interval=%d min", v);
        }
    } else if (strcmp(nvs_key, NVS_KEY_SLP) == 0) {
        int32_t v = (*val == '1') ? 1 : 0;
        nvs_set_i32(h, nvs_key, v);
        ESP_LOGI(TAG, "config: sleep_enabled=%d", (int)v);
    }

    nvs_commit(h);
    nvs_close(h);
}

static void apply_image_config_headers(esp_http_client_handle_t client)
{
    camera_image_config_t config;
    camera_image_config_load(&config);
    char *val = NULL;

    esp_http_client_get_header(client, "X-Config-Framesize", &val);
    if (val && *val) strlcpy(config.framesize, val, sizeof(config.framesize));
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Quality", &val);
    if (val && *val) config.quality = atoi(val);
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Brightness", &val);
    if (val && *val) config.brightness = atoi(val);
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Contrast", &val);
    if (val && *val) config.contrast = atoi(val);
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Saturation", &val);
    if (val && *val) config.saturation = atoi(val);
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Sharpness", &val);
    if (val && *val) config.sharpness = atoi(val);
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Hmirror", &val);
    if (val && *val) config.hmirror = *val == '1';
    val = NULL;
    esp_http_client_get_header(client, "X-Config-Vflip", &val);
    if (val && *val) config.vflip = *val == '1';

    esp_err_t err = camera_image_config_apply(&config, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ignoring invalid image config headers: %s",
                 esp_err_to_name(err));
    }
}

esp_err_t http_client_post_frame(const uint8_t *buf, size_t len,
                                  const char *trigger)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/api/frames",
             CONFIG_M5CAM_SERVER_IP, CONFIG_M5CAM_SERVER_PORT);

    char ts[32] = "unknown";
    struct tm t;
    if (bm8563_get_time(&t) == ESP_OK) {
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &t);
    }

    esp_http_client_config_t cfg = {
        .url        = url,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_header(client, "X-Captured-At", ts);
    esp_http_client_set_header(client, "X-Trigger", trigger);
    esp_http_client_set_post_field(client, (const char *)buf, (int)len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST %s → %d", url, status);
        if (status >= 200 && status < 300) {
            apply_config_header(client, "X-Config-Interval", NVS_KEY_INT);
            apply_config_header(client, "X-Config-Sleep",    NVS_KEY_SLP);
            apply_image_config_headers(client);
        } else {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
