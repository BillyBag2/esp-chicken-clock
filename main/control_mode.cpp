#include "control_mode.hpp"

#include <cstdint>

#include "nvs.h"

namespace {
constexpr char kNamespace[] = "control";
constexpr char kModeKey[] = "mode";
control_mode_t s_mode = control_mode_t::kTimer;
bool s_loaded = false;
bool s_stored = false;
}

bool control_mode_load(control_mode_t *mode)
{
    if (s_loaded) {
        *mode = s_mode;
        return s_stored;
    }
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        s_loaded = true;
        s_mode = control_mode_t::kTimer;
        *mode = s_mode;
        return false;
    }
    uint8_t value = 0;
    const esp_err_t result = nvs_get_u8(handle, kModeKey, &value);
    nvs_close(handle);
    if (result != ESP_OK || value > static_cast<uint8_t>(control_mode_t::kOff)) {
        s_loaded = true;
        s_mode = control_mode_t::kTimer;
        *mode = s_mode;
        return false;
    }
    s_loaded = true;
    s_stored = true;
    s_mode = static_cast<control_mode_t>(value);
    *mode = s_mode;
    return true;
}

bool control_mode_save(control_mode_t mode)
{
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t set_result = nvs_set_u8(handle, kModeKey, static_cast<uint8_t>(mode));
    const esp_err_t commit_result = nvs_commit(handle);
    nvs_close(handle);
    if (set_result != ESP_OK || commit_result != ESP_OK) return false;
    s_mode = mode;
    s_loaded = true;
    s_stored = true;
    return true;
}

const char *control_mode_name(control_mode_t mode)
{
    switch (mode) {
    case control_mode_t::kTimer: return "TIMER";
    case control_mode_t::kOn: return "ON";
    case control_mode_t::kOff: return "OFF";
    }
    return "TIMER";
}
