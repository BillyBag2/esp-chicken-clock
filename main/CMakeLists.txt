idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "favicon.ico"
                    PRIV_REQUIRES esp_driver_gpio esp_event esp_http_server esp_netif esp_wifi mdns nvs_flash)
