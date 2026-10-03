
idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        lwip
        esp_netif
        esp_wifi
        esp_event
        nvs_flash
        esp_driver_uart
        esp_hw_support
        esp_timer
)

idf_component_get_property(lwip_lib lwip COMPONENT_LIB)
