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

/* ── Response-header capture ───────────────────────────────────────────── */

/* Holds the config values found in the server's POST response headers.
 * Fields are zero-initialised; a non-empty string means the header arrived. */
typedef struct {
    char interval[8];
    char framesize[8];
    char quality[4];
    char brightness[4];
    char contrast[4];
    char saturation[4];
    char sharpness[4];
    char hmirror[4];
    char vflip[4];
    char power_off[4];
} resp_cfg_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;
    resp_cfg_t *r = (resp_cfg_t *)evt->user_data;
    const char *k = evt->header_key;
    const char *v = evt->header_value;

    if      (strcasecmp(k, "X-Config-Interval")   == 0) strlcpy(r->interval,   v, sizeof(r->interval));
    else if (strcasecmp(k, "X-Config-Framesize")   == 0) strlcpy(r->framesize,  v, sizeof(r->framesize));
    else if (strcasecmp(k, "X-Config-Quality")     == 0) strlcpy(r->quality,    v, sizeof(r->quality));
    else if (strcasecmp(k, "X-Config-Brightness")  == 0) strlcpy(r->brightness, v, sizeof(r->brightness));
    else if (strcasecmp(k, "X-Config-Contrast")    == 0) strlcpy(r->contrast,   v, sizeof(r->contrast));
    else if (strcasecmp(k, "X-Config-Saturation")  == 0) strlcpy(r->saturation, v, sizeof(r->saturation));
    else if (strcasecmp(k, "X-Config-Sharpness")   == 0) strlcpy(r->sharpness,  v, sizeof(r->sharpness));
    else if (strcasecmp(k, "X-Config-Hmirror")     == 0) strlcpy(r->hmirror,    v, sizeof(r->hmirror));
    else if (strcasecmp(k, "X-Config-Vflip")       == 0) strlcpy(r->vflip,      v, sizeof(r->vflip));
    else if (strcasecmp(k, "X-Config-Power-Off")   == 0) strlcpy(r->power_off,  v, sizeof(r->power_off));

    return ESP_OK;
}

/* ── NVS helpers ───────────────────────────────────────────────────────── */

static void apply_nvs_int(const char *val, const char *key, int min, int max)
{
    if (!val || !*val) return;
    int v = atoi(val);
    if (v < min || v > max) return;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, key, (int32_t)v);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "config: %s=%d", key, v);
}

/* ── Apply response config ─────────────────────────────────────────────── */

static void apply_resp_config(const resp_cfg_t *r)
{
    if (r->power_off[0] == '1') {
        ESP_LOGI(TAG, "power off requested by server, powering off now");
        bm8563_power_off();
    }

    apply_nvs_int(r->interval, NVS_KEY_INT, 1, 1440);

    if (!r->framesize[0]) return;  /* no image config headers → skip */

    camera_image_config_t img;
    camera_image_config_load(&img);
    bool changed = false;

    if (r->framesize[0])  { strlcpy(img.framesize, r->framesize, sizeof(img.framesize)); changed = true; }
    if (r->quality[0])    { img.quality    = atoi(r->quality);    changed = true; }
    if (r->brightness[0]) { img.brightness = atoi(r->brightness); changed = true; }
    if (r->contrast[0])   { img.contrast   = atoi(r->contrast);   changed = true; }
    if (r->saturation[0]) { img.saturation = atoi(r->saturation); changed = true; }
    if (r->sharpness[0])  { img.sharpness  = atoi(r->sharpness);  changed = true; }
    if (r->hmirror[0])    { img.hmirror    = r->hmirror[0] == '1'; changed = true; }
    if (r->vflip[0])      { img.vflip      = r->vflip[0]   == '1'; changed = true; }

    if (changed) {
        esp_err_t err = camera_image_config_apply(&img, true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ignoring invalid image config from server: %s",
                     esp_err_to_name(err));
        }
    }
}

/* ── POST a frame ──────────────────────────────────────────────────────── */

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

    resp_cfg_t resp = {0};

    esp_http_client_config_t cfg = {
        .url           = url,
        .method        = HTTP_METHOD_POST,
        .timeout_ms    = 10000,
        .event_handler = http_event_handler,
        .user_data     = &resp,
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
            apply_resp_config(&resp);
        } else {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
