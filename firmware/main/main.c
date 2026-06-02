#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/adc.h"

#include "camera.h"
#include "wifi.h"
#include "http_client.h"
#include "http_server.h"
#include "bm8563.h"
#include "led.h"

static const char *TAG = "main";

/* ── Battery ADC (GPIO38 = ADC1_CH2, 100k/100k divider) ───────────────── */
static int battery_mv(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_12);
    int raw = adc1_get_raw(ADC1_CHANNEL_2);
    return (int)((float)raw * 4900.0f / 4095.0f);
}

/* Heuristic: if battery voltage > 4.10 V the device is likely charging via USB */
static bool usb_likely_connected(void)
{
    return battery_mv() > 4100;
}

/* ── Capture + POST one frame ──────────────────────────────────────────── */
static void capture_and_post(const char *trigger)
{
    uint8_t *buf = NULL;
    size_t   len = 0;

    esp_err_t err = camera_capture(&buf, &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "capture failed: %s", esp_err_to_name(err));
        led_blink(LED_ERROR);
        return;
    }
    ESP_LOGI(TAG, "captured %zu bytes", len);
    led_blink(LED_CAPTURE_OK);

    err = http_client_post_frame(buf, len, trigger);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
        led_blink(LED_ERROR);
    } else {
        ESP_LOGI(TAG, "frame posted OK");
    }

    camera_release(buf);
}

/* ── Awake loop: used when sleep is disabled or USB is connected ─────── */
static void run_awake_loop(int interval_minutes)
{
    ESP_LOGI(TAG, "awake mode — interval %d min (sleep disabled/USB)", interval_minutes);
    http_server_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)interval_minutes * 60 * 1000));

        /* Re-read config each cycle so live updates take effect */
        interval_minutes = cam_config_get_interval();
        if (!cam_config_get_sleep_enabled() || usb_likely_connected()) {
            capture_and_post("timer");
        } else {
            /* Sleep re-enabled and USB disconnected — restart to apply */
            ESP_LOGI(TAG, "sleep re-enabled — rebooting");
            esp_restart();
        }
    }
}

/* ── Test mode loop ────────────────────────────────────────────────────── */
#if CONFIG_M5CAM_TEST_MODE
static void run_test_loop(void)
{
    ESP_LOGI(TAG, "TEST MODE — capturing every 2s, no deep sleep");

    if (wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi failed");
        led_blink(LED_ERROR);
    }
    http_server_start();

    while (1) {
        capture_and_post("test");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif /* CONFIG_M5CAM_TEST_MODE */

/* ── app_main ──────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(bm8563_init());
    ESP_ERROR_CHECK(camera_init());
    led_blink(LED_SLEEPING);

#if CONFIG_M5CAM_TEST_MODE
    run_test_loop();
#else
    /* Connect WiFi, capture, post */
    led_blink(LED_WIFI_CONNECTING);
    if (wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi failed — skipping cycle");
        led_blink(LED_ERROR);
        goto sleep;
    }

    capture_and_post("timer");

    /* Read config from NVS (may have been updated by the POST response) */
    int  interval = cam_config_get_interval();
    bool sleep_en = cam_config_get_sleep_enabled();
    bool usb      = usb_likely_connected();

    ESP_LOGI(TAG, "interval=%dmin sleep_en=%d usb=%d",
             interval, (int)sleep_en, (int)usb);

    if (!sleep_en || usb) {
        /* Stay awake: keep WiFi + HTTP server running, capture periodically */
        run_awake_loop(interval);
        /* run_awake_loop never returns */
    }

    /* Normal production: brief HTTP server window then deep sleep */
    http_server_start();
    vTaskDelay(pdMS_TO_TICKS(5000));
    http_server_stop();

sleep:
    ESP_LOGI(TAG, "entering deep sleep for %d minutes", cam_config_get_interval());
    led_blink(LED_SLEEPING);
    bm8563_set_wake_alarm(cam_config_get_interval());
    /* Battery hold released inside bm8563_set_wake_alarm */
#endif
}
