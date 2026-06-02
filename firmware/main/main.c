#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "camera.h"
#include "wifi.h"
#include "http_client.h"
#include "http_server.h"
#include "rtc.h"
#include "led.h"

static const char *TAG = "main";

static void run_capture_cycle(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;

    led_blink(LED_WIFI_CONNECTING);
    ESP_ERROR_CHECK(wifi_connect());

    led_blink(LED_CAPTURE_OK);
    esp_err_t err = camera_capture(&buf, &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "capture failed: %s", esp_err_to_name(err));
        led_blink(LED_ERROR);
        goto done;
    }
    ESP_LOGI(TAG, "captured %zu bytes", len);

    err = http_client_post_frame(buf, len, "timer");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
        led_blink(LED_ERROR);
    } else {
        ESP_LOGI(TAG, "frame posted OK");
    }

    camera_release(buf);

done:
    wifi_disconnect();
}

static void run_test_loop(void)
{
    ESP_LOGI(TAG, "TEST MODE — capturing every 2s, no deep sleep");

    ESP_ERROR_CHECK(wifi_connect());
    ESP_ERROR_CHECK(http_server_start());

    while (1) {
        uint8_t *buf = NULL;
        size_t   len = 0;

        esp_err_t err = camera_capture(&buf, &len);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "captured %zu bytes", len);
            led_blink(LED_CAPTURE_OK);
            err = http_client_post_frame(buf, len, "test");
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "POST OK");
            } else {
                ESP_LOGW(TAG, "POST failed: %s", esp_err_to_name(err));
                led_blink(LED_ERROR);
            }
            camera_release(buf);
        } else {
            ESP_LOGE(TAG, "capture failed: %s", esp_err_to_name(err));
            led_blink(LED_ERROR);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(rtc_init());
    ESP_ERROR_CHECK(camera_init());
    led_blink(LED_SLEEPING); /* boot indicator */

#if CONFIG_M5CAM_TEST_MODE
    run_test_loop();
#else
    run_capture_cycle();

    /* Keep HTTP server alive briefly so server can pull a snapshot */
    ESP_ERROR_CHECK(http_server_start());
    vTaskDelay(pdMS_TO_TICKS(5000));
    http_server_stop();

    ESP_LOGI(TAG, "entering deep sleep for %d minutes",
             CONFIG_M5CAM_CAPTURE_INTERVAL_MINUTES);
    led_blink(LED_SLEEPING);
    rtc_set_wake_alarm(CONFIG_M5CAM_CAPTURE_INTERVAL_MINUTES);
    /* Battery hold release handled inside rtc_set_wake_alarm */
#endif
}
