#pragma once

#include "driver/gpio.h"

namespace board_pins {

// ESP32_MOS_X2_V1.1 board connections.
constexpr gpio_num_t kLed = GPIO_NUM_23;
constexpr gpio_num_t kMosfet1 = GPIO_NUM_16;
constexpr gpio_num_t kMosfet2 = GPIO_NUM_17;
constexpr gpio_num_t kIo0Button = GPIO_NUM_0;

}  // namespace board_pins
