idf_component_register(
    SRCS "http_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server camera app_update esp_system esp_wifi nvs_flash driver
)
