#pragma once

#include "wifi.hpp"

void led_initialize();
void led_update(connection_state_t connection_state);
void led_set_mosfet1(bool on);
