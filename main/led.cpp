#include "led.hpp"

#include <cstdint>

#include "board_pins.hpp"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr uint32_t kConnectingPeriodMs = 1'000;
constexpr uint32_t kAccessPointPeriodMs = 200;
constexpr uint32_t kConnectedPeriodMs = 1'000;
constexpr uint32_t kConnectedBlipMs = 100;

void configure_output(gpio_num_t pin) {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&config));
}
}

void led_initialize()
{
    configure_output(board_pins::kLed);
    configure_output(board_pins::kMosfet1);
    configure_output(board_pins::kMosfet2);
    ESP_ERROR_CHECK(gpio_set_level(board_pins::kMosfet1, 0));
    ESP_ERROR_CHECK(gpio_set_level(board_pins::kMosfet2, 0));

    gpio_config_t button_config = {};
    button_config.pin_bit_mask = 1ULL << board_pins::kIo0Button;
    button_config.mode = GPIO_MODE_INPUT;
    button_config.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&button_config));
}

void led_update(connection_state_t state)
{
    const uint32_t elapsed_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool on = false;
    if (gpio_get_level(board_pins::kIo0Button) != 0) {
        switch (state) {
        case connection_state_t::kConnecting:
            on = elapsed_ms % kConnectingPeriodMs < kConnectingPeriodMs / 2;
            break;
        case connection_state_t::kConnected:
            on = elapsed_ms % kConnectedPeriodMs < kConnectedBlipMs;
            break;
        case connection_state_t::kAccessPoint:
            on = elapsed_ms % kAccessPointPeriodMs < kAccessPointPeriodMs / 2;
            break;
        }
    }
    ESP_ERROR_CHECK(gpio_set_level(board_pins::kLed, on ? 1 : 0));
}

void led_set_mosfet1(bool on)
{
    ESP_ERROR_CHECK(gpio_set_level(board_pins::kMosfet1, on ? 1 : 0));
}
