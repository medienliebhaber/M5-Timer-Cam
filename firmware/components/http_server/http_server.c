#include "http_server.h"
#include "bm8563.h"
#include "camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "driver/adc.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "http_server";
static httpd_handle_t s_server = NULL;

#define OTA_BUF_SIZE  1024
#define NVS_NS        "m5cam"
#define NVS_KEY_INT   "interval"

/* ── NVS helpers ───────────────────────────────────────────────────────── */
int cam_config_get_interval(void)
{
    nvs_handle_t h;
    int32_t val = CONFIG_M5CAM_CAPTURE_INTERVAL_MINUTES;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, NVS_KEY_INT, &val);
        nvs_close(h);
    }
    return (int)val;
}

static void cfg_write_i32(const char *key, int32_t val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, key, val);
    nvs_commit(h);
    nvs_close(h);
}

static int recv_body(httpd_req_t *req, char *body, size_t size)
{
    if (req->content_len <= 0 || req->content_len >= size) return -1;
    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (ret <= 0) return -1;
        received += ret;
    }
    body[received] = '\0';
    return received;
}

/* ── Battery ADC (GPIO38 = ADC1_CH2, 100k/100k divider) ───────────────── */
static int read_battery_mv(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_12);
    int raw = adc1_get_raw(ADC1_CHANNEL_2);
    /* 11dB atten: input range 0–2450 mV; divider doubles actual voltage */
    return (int)((float)raw * 4900.0f / 4095.0f);
}

/* ── GET /snapshot ─────────────────────────────────────────────────────── */
static esp_err_t snapshot_handler(httpd_req_t *req)
{
    uint8_t *buf = NULL;
    size_t   len = 0;

    esp_err_t err = camera_capture(&buf, &len);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    err = httpd_resp_send(req, (const char *)buf, (ssize_t)len);
    camera_release(buf);
    return err;
}

static const httpd_uri_t SNAPSHOT_URI = {
    .uri      = "/snapshot",
    .method   = HTTP_GET,
    .handler  = snapshot_handler,
    .user_ctx = NULL,
};

/* ── GET /status ───────────────────────────────────────────────────────── */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    uint32_t heap_free = esp_get_free_heap_size();
    uint32_t heap_min  = esp_get_minimum_free_heap_size();

    int rssi = 0;
    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    int vbat_mv    = read_battery_mv();
    int battery_pct = (vbat_mv - 3300) * 100 / (4200 - 3300);
    if (battery_pct < 0)   battery_pct = 0;
    if (battery_pct > 100) battery_pct = 100;

    char buf[192];
    snprintf(buf, sizeof(buf),
        "{\"heap_free\":%lu,\"heap_min\":%lu,\"rssi\":%d,"
        "\"battery_pct\":%d,\"battery_mv\":%d}",
        (unsigned long)heap_free, (unsigned long)heap_min,
        rssi, battery_pct, vbat_mv);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, buf);
}

static const httpd_uri_t STATUS_URI = {
    .uri      = "/status",
    .method   = HTTP_GET,
    .handler  = status_get_handler,
    .user_ctx = NULL,
};

static bool image_config_update(cJSON *root, camera_image_config_t *config,
                                bool *changed)
{
    cJSON *item;
#define UPDATE_INT(name) do { \
    item = cJSON_GetObjectItemCaseSensitive(root, #name); \
    if (item) { \
        if (!cJSON_IsNumber(item)) return false; \
        config->name = item->valueint; \
        *changed = true; \
    } \
} while (0)
#define UPDATE_BOOL(name) do { \
    item = cJSON_GetObjectItemCaseSensitive(root, #name); \
    if (item) { \
        if (!cJSON_IsBool(item)) return false; \
        config->name = cJSON_IsTrue(item); \
        *changed = true; \
    } \
} while (0)

    item = cJSON_GetObjectItemCaseSensitive(root, "framesize");
    if (item) {
        if (!cJSON_IsString(item) || !item->valuestring) return false;
        strlcpy(config->framesize, item->valuestring, sizeof(config->framesize));
        *changed = true;
    }
    UPDATE_INT(quality);
    UPDATE_INT(brightness);
    UPDATE_INT(contrast);
    UPDATE_INT(saturation);
    UPDATE_INT(sharpness);
    UPDATE_BOOL(hmirror);
    UPDATE_BOOL(vflip);
#undef UPDATE_INT
#undef UPDATE_BOOL
    return true;
}

/* ── GET /config ───────────────────────────────────────────────────────── */
static esp_err_t config_get_handler(httpd_req_t *req)
{
    int  interval = cam_config_get_interval();
    camera_image_config_t image;
    if (camera_image_config_get(&image) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera config unavailable");
        return ESP_FAIL;
    }

    char buf[320];
    snprintf(buf, sizeof(buf),
        "{\"interval_minutes\":%d,"
        "\"framesize\":\"%s\",\"quality\":%d,\"brightness\":%d,"
        "\"contrast\":%d,\"saturation\":%d,\"sharpness\":%d,"
        "\"hmirror\":%s,\"vflip\":%s}",
        interval, image.framesize,
        image.quality, image.brightness, image.contrast, image.saturation,
        image.sharpness, image.hmirror ? "true" : "false",
        image.vflip ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, buf);
}

static const httpd_uri_t CONFIG_GET_URI = {
    .uri      = "/config",
    .method   = HTTP_GET,
    .handler  = config_get_handler,
    .user_ctx = NULL,
};

/* ── POST /config ──────────────────────────────────────────────────────── */
static esp_err_t config_post_handler(httpd_req_t *req)
{
    char body[512] = {0};
    int received = recv_body(req, body, sizeof(body));
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "interval_minutes");
    bool interval_changed = item != NULL;
    int interval = 0;
    if (item) {
        if (!cJSON_IsNumber(item) || item->valueint < 1 || item->valueint > 1440) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid interval");
            return ESP_FAIL;
        }
        interval = item->valueint;
    }

    camera_image_config_t image;
    camera_image_config_get(&image);
    bool image_changed = false;
    if (!image_config_update(root, &image, &image_changed)
        || (image_changed && camera_image_config_apply(&image, true) != ESP_OK)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid image config");
        return ESP_FAIL;
    }
    if (interval_changed) cfg_write_i32(NVS_KEY_INT, interval);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

static const httpd_uri_t CONFIG_POST_URI = {
    .uri      = "/config",
    .method   = HTTP_POST,
    .handler  = config_post_handler,
    .user_ctx = NULL,
};

/* ── POST /preview ─────────────────────────────────────────────────────── */
static esp_err_t preview_post_handler(httpd_req_t *req)
{
    char body[512] = {0};
    int received = recv_body(req, body, sizeof(body));
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }
    camera_image_config_t image;
    camera_image_config_get(&image);
    bool changed = false;
    bool valid = image_config_update(root, &image, &changed);
    cJSON_Delete(root);
    if (!valid) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid image config");
        return ESP_FAIL;
    }
    uint8_t *buf = NULL;
    size_t len = 0;
    esp_err_t err = camera_preview_capture(&image, &buf, &len);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "preview failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    err = httpd_resp_send(req, (const char *)buf, (ssize_t)len);
    camera_release(buf);
    return err;
}

static const httpd_uri_t PREVIEW_URI = {
    .uri      = "/preview",
    .method   = HTTP_POST,
    .handler  = preview_post_handler,
    .user_ctx = NULL,
};

/* ── POST /ota ─────────────────────────────────────────────────────────── */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no OTA partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(update, OTA_SIZE_UNKNOWN, &handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_begin failed");
        return ESP_FAIL;
    }

    char *buf = malloc(OTA_BUF_SIZE);
    if (!buf) {
        esp_ota_abort(handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "malloc failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    ESP_LOGI(TAG, "OTA start — %d bytes", remaining);

    while (remaining > 0) {
        int to_read  = remaining < OTA_BUF_SIZE ? remaining : OTA_BUF_SIZE;
        int received = httpd_req_recv(req, buf, to_read);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (received <= 0) {
            free(buf); esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        if (esp_ota_write(handle, buf, received) != ESP_OK) {
            free(buf); esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write failed");
            return ESP_FAIL;
        }
        remaining -= received;
    }
    free(buf);

    if (esp_ota_end(handle) != ESP_OK || esp_ota_set_boot_partition(update) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_end failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA success — rebooting");
    httpd_resp_sendstr(req, "OK");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

static const httpd_uri_t OTA_URI = {
    .uri      = "/ota",
    .method   = HTTP_POST,
    .handler  = ota_post_handler,
    .user_ctx = NULL,
};

/* ── POST /power-off ──────────────────────────────────────────────────── */
static esp_err_t power_off_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, "{\"status\":\"powering_off\"}");
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(300));
    err = bm8563_power_off();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "power off failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static const httpd_uri_t POWER_OFF_URI = {
    .uri      = "/power-off",
    .method   = HTTP_POST,
    .handler  = power_off_post_handler,
    .user_ctx = NULL,
};

/* ── Server lifecycle ──────────────────────────────────────────────────── */
esp_err_t http_server_start(void)
{
    if (s_server) return ESP_OK;

    httpd_config_t cfg      = HTTPD_DEFAULT_CONFIG();
    cfg.server_port         = 80;
    cfg.recv_wait_timeout   = 30;
    cfg.send_wait_timeout   = 30;
    cfg.max_uri_handlers    = 10;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_register_uri_handler(s_server, &SNAPSHOT_URI);
    httpd_register_uri_handler(s_server, &STATUS_URI);
    httpd_register_uri_handler(s_server, &CONFIG_GET_URI);
    httpd_register_uri_handler(s_server, &CONFIG_POST_URI);
    httpd_register_uri_handler(s_server, &PREVIEW_URI);
    httpd_register_uri_handler(s_server, &OTA_URI);
    httpd_register_uri_handler(s_server, &POWER_OFF_URI);
    ESP_LOGI(TAG, "started on port 80 (/snapshot /status /config /preview /ota /power-off)");
    return ESP_OK;
}

void http_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
