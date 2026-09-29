idf_component_register(SRCS "sun_schedule.cpp" "sun_settings.cpp" "location.cpp" "main.cpp" "led.cpp" "time_service.cpp" "web_server.cpp" "wifi.cpp"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "favicon.ico"
                    PRIV_REQUIRES esp_driver_gpio esp_event esp_http_client esp_http_server esp_netif esp_wifi mdns nvs_flash)
