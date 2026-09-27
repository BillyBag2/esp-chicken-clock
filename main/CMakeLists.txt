idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_gpio esp_event esp_http_server esp_netif esp_wifi nvs_flash)
