#pragma once

#include <cstdint>

struct sun_settings_t {
    int8_t sunset_offset_minutes;
    int8_t dusk_offset_minutes;
};

bool sun_settings_load(sun_settings_t *settings);
bool sun_settings_save(const sun_settings_t &settings);
