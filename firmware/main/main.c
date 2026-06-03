#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
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

/* True if USB is powering the board.
 *
 * Primary signal: if the ESP32 woke from its own deep-sleep timer, GPIO33
 * (BAT_HOLD) was already released.  On battery the board would have lost all
 * power at that point, so surviving the sleep means USB is keeping us alive.
 * Fallback for fresh power-on: battery voltage above 4.10 V implies charging. */
static bool usb_likely_connected(void)
{
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        return true;
    }
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

/* ── Awake loop: used while USB power is connected ──────────────────────── */
static void run_awake_loop(int interval_minutes)
{
    ESP_LOGI(TAG, "awake mode — interval %d min (USB connected)", interval_minutes);
    http_server_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)interval_minutes * 60 * 1000));

        /* Re-read interval each cycle so live updates take effect */
        interval_minutes = cam_config_get_interval();
        if (usb_likely_connected()) {
            capture_and_post("timer");
        } else {
            /* USB unplugged — restart into the battery power-off flow */
            ESP_LOGI(TAG, "USB disconnected — rebooting to battery mode");
            esp_restart();
        }
    }
}

/* ── Arm the RTC wake timer and power off (battery + WiFi-fail paths) ────── */
static void enter_scheduled_sleep(void)
{
    int interval = cam_config_get_interval();
    ESP_LOGI(TAG, "entering deep sleep for %d minutes", interval);
    led_blink(LED_SLEEPING);
    esp_err_t err = bm8563_set_wake_alarm(interval);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to arm RTC wake timer: %s", esp_err_to_name(err));
        led_blink(LED_ERROR);
        vTaskDelay(pdMS_TO_TICKS(60000));
        esp_restart();
    }
    /* Battery hold released inside bm8563_set_wake_alarm */
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
    if (camera_init(FB_COUNT_STREAM) != ESP_OK) {
        ESP_LOGE(TAG, "camera init failed");
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
    led_blink(LED_SLEEPING);

#if CONFIG_M5CAM_TEST_MODE
    run_test_loop();
#else
    /* USB connected → stay awake (full UX). On battery → capture once + power off.
     * Decided up front so the camera can be initialised with the right buffer
     * count, and so the sensor never powers on during WiFi association. */
    bool usb = usb_likely_connected();

    led_blink(LED_WIFI_CONNECTING);
    if (wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi failed — skipping cycle");
        led_blink(LED_ERROR);
        enter_scheduled_sleep();
        return;
    }

    /* Camera powers on only after WiFi is up (saves the association window). */
    if (camera_init(usb ? FB_COUNT_STREAM : FB_COUNT_ONESHOT) != ESP_OK) {
        ESP_LOGE(TAG, "camera init failed — skipping cycle");
        led_blink(LED_ERROR);
        enter_scheduled_sleep();
        return;
    }

    capture_and_post("timer");

    /* Re-check USB after the POST (response may have changed the interval). */
    if (usb_likely_connected()) {
        /* Stay awake: keep WiFi + HTTP server running, capture periodically. */
        run_awake_loop(cam_config_get_interval());
        /* run_awake_loop never returns */
    }

    /* Battery: no live server window — go straight to power off. */
    enter_scheduled_sleep();
#endif
}
