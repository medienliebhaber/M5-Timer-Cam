#include "http_server.h"
#include "camera.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "http_server";
static httpd_handle_t s_server = NULL;

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

esp_err_t http_server_start(void)
{
    if (s_server) {
        return ESP_OK; /* already running */
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_register_uri_handler(s_server, &SNAPSHOT_URI);
    ESP_LOGI(TAG, "started on port 80");
    return ESP_OK;
}

void http_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
