#include "http_client.h"
#include "rtc.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "http_client";

esp_err_t http_client_post_frame(const uint8_t *buf, size_t len,
                                  const char *trigger)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/api/frames",
             CONFIG_M5CAM_SERVER_IP, CONFIG_M5CAM_SERVER_PORT);

    /* ISO8601 timestamp from RTC */
    char ts[32] = "unknown";
    struct tm t;
    if (rtc_get_time(&t) == ESP_OK) {
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &t);
    }

    esp_http_client_config_t cfg = {
        .url            = url,
        .method         = HTTP_METHOD_POST,
        .timeout_ms     = 10000,
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
        if (status < 200 || status >= 300) {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
