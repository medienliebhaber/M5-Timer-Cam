#include "rtc.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "rtc";

/* BM8563 I2C address and register map */
#define BM8563_ADDR     0x51
#define BM8563_REG_SEC  0x02

#define RTC_SCL_PIN     14
#define RTC_SDA_PIN     12
#define RTC_I2C_PORT    I2C_NUM_0

#define BAT_HOLD_PIN    33

static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

esp_err_t rtc_init(void)
{
    /* Keep battery power on while MCU is active */
    gpio_config_t bat = {
        .pin_bit_mask = 1ULL << BAT_HOLD_PIN,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bat);
    gpio_set_level(BAT_HOLD_PIN, 1);

    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = RTC_SDA_PIN,
        .scl_io_num       = RTC_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(RTC_I2C_PORT, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(RTC_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    ESP_LOGI(TAG, "BM8563 ready");
    return ESP_OK;
}

esp_err_t rtc_get_time(struct tm *t)
{
    uint8_t reg = BM8563_REG_SEC;
    uint8_t data[7];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 7, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(RTC_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        return err;
    }

    memset(t, 0, sizeof(*t));
    t->tm_sec  = bcd2dec(data[0] & 0x7F);
    t->tm_min  = bcd2dec(data[1] & 0x7F);
    t->tm_hour = bcd2dec(data[2] & 0x3F);
    t->tm_mday = bcd2dec(data[3] & 0x3F);
    t->tm_mon  = bcd2dec(data[5] & 0x1F) - 1;
    t->tm_year = bcd2dec(data[6]) + 100; /* years since 1900 */
    return ESP_OK;
}

esp_err_t rtc_set_wake_alarm(int minutes_from_now)
{
    /* Use ESP32 internal timer for deep sleep (simpler than BM8563 alarm wiring).
     * The BM8563 keeps time during sleep; on wake we read it for timestamps. */
    uint64_t sleep_us = (uint64_t)minutes_from_now * 60ULL * 1000000ULL;
    ESP_LOGI(TAG, "sleeping for %d min (%llu us)", minutes_from_now, sleep_us);

    esp_sleep_enable_timer_wakeup(sleep_us);

    /* Release battery hold → allows RTC to cut power to ESP32 on the board's
     * power circuit, achieving the ~2µA sleep current. */
    gpio_set_level(BAT_HOLD_PIN, 0);

    esp_deep_sleep_start();
    return ESP_OK; /* never reached */
}
