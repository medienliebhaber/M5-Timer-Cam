#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN 2

static bool s_init = false;

static void ensure_init(void)
{
    if (s_init) return;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(LED_PIN, 0);
    s_init = true;
}

static void pulse(int on_ms, int off_ms, int count)
{
    for (int i = 0; i < count; i++) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

void led_init(void)
{
    ensure_init();
}

void led_blink(led_pattern_t pattern)
{
    ensure_init();
    switch (pattern) {
    case LED_WIFI_CONNECTING:
        pulse(100, 100, 5);
        break;
    case LED_CAPTURE_OK:
        pulse(80, 80, 2);
        break;
    case LED_ERROR:
        pulse(50, 50, 5);
        break;
    case LED_SLEEPING:
        pulse(500, 0, 1);
        break;
    }
}
