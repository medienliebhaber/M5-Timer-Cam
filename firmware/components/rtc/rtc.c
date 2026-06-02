#include "bm8563.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "rtc";

/* BM8563 I2C address and register map */
#define BM8563_ADDR              0x51
#define BM8563_REG_CTRL_STATUS2  0x01
#define BM8563_REG_SEC           0x02
#define BM8563_REG_TIMER_CTRL    0x0E
#define BM8563_REG_TIMER         0x0F

#define BM8563_CTRL_STATUS2_TIMER_INT_ENABLE  0x01
#define BM8563_TIMER_CTRL_ENABLE              0x80
#define BM8563_TIMER_CTRL_1HZ                 0x02
#define BM8563_TIMER_CTRL_1_60HZ              0x03

#define RTC_SCL_PIN     14
#define RTC_SDA_PIN     12
#define RTC_I2C_PORT    I2C_NUM_0

#define BAT_HOLD_PIN    33

static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }

static esp_err_t bm8563_write_reg(uint8_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(RTC_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

esp_err_t bm8563_init(void)
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

esp_err_t bm8563_get_time(struct tm *t)
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

esp_err_t bm8563_set_wake_alarm(int minutes_from_now)
{
    if (minutes_from_now < 1 || minutes_from_now > 255) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "powering off for %d min using BM8563 timer", minutes_from_now);

    uint8_t timer_ctrl = BM8563_TIMER_CTRL_ENABLE | BM8563_TIMER_CTRL_1_60HZ;
    uint8_t timer_value = (uint8_t)minutes_from_now;
    if (minutes_from_now <= 4) {
        timer_ctrl = BM8563_TIMER_CTRL_ENABLE | BM8563_TIMER_CTRL_1HZ;
        timer_value = (uint8_t)(minutes_from_now * 60);
    }

    esp_err_t err = bm8563_write_reg(BM8563_REG_TIMER_CTRL, 0);
    if (err != ESP_OK) return err;
    err = bm8563_write_reg(BM8563_REG_CTRL_STATUS2,
                           BM8563_CTRL_STATUS2_TIMER_INT_ENABLE);
    if (err != ESP_OK) return err;
    err = bm8563_write_reg(BM8563_REG_TIMER, timer_value);
    if (err != ESP_OK) return err;
    err = bm8563_write_reg(BM8563_REG_TIMER_CTRL, timer_ctrl);
    if (err != ESP_OK) return err;

    /* The BM8563 interrupt powers the board back on when battery-powered.
     * The ESP32 timer wakes a board that remains powered over USB. */
    esp_sleep_enable_timer_wakeup((uint64_t)minutes_from_now * 60ULL * 1000000ULL);
    err = gpio_set_level(BAT_HOLD_PIN, 0);
    if (err != ESP_OK) return err;

    esp_deep_sleep_start();
    return ESP_OK; /* never reached while the ESP32 remains powered */
}

esp_err_t bm8563_power_off(void)
{
    esp_err_t err = bm8563_write_reg(BM8563_REG_TIMER_CTRL, 0);
    if (err != ESP_OK) return err;

    err = gpio_set_level(BAT_HOLD_PIN, 0);
    if (err != ESP_OK) return err;

    esp_deep_sleep_start();
    return ESP_OK; /* never reached while the ESP32 remains powered */
}
