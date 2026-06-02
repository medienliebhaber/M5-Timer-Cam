idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        camera
        wifi
        http_client
        http_server
        rtc
        led
        esp_timer
        nvs_flash
        driver
)
