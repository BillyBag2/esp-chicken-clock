#include "led.hpp"
#include "sun_schedule.hpp"
#include "web_server.hpp"
#include "wifi.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    led_initialize();
    wifi_initialize();
    wifi_start();
    web_server_start();

    while (true) {
        wifi_update();
        sun_schedule_update();
        led_update(wifi_connection_state());
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
