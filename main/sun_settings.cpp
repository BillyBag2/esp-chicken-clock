#include "sun_settings.hpp"

#include "nvs.h"

namespace {
constexpr char kNamespace[] = "sun";
constexpr char kSunsetOffsetKey[] = "sunset_offset";
constexpr char kDuskOffsetKey[] = "dusk_offset";
}

bool sun_settings_load(sun_settings_t *settings)
{
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
    const esp_err_t sunset_result = nvs_get_i8(handle, kSunsetOffsetKey, &settings->sunset_offset_minutes);
    const esp_err_t dusk_result = nvs_get_i8(handle, kDuskOffsetKey, &settings->dusk_offset_minutes);
    nvs_close(handle);
    return sunset_result == ESP_OK && dusk_result == ESP_OK;
}

bool sun_settings_save(const sun_settings_t &settings)
{
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t sunset_result = nvs_set_i8(handle, kSunsetOffsetKey, settings.sunset_offset_minutes);
    const esp_err_t dusk_result = nvs_set_i8(handle, kDuskOffsetKey, settings.dusk_offset_minutes);
    const esp_err_t commit_result = nvs_commit(handle);
    nvs_close(handle);
    return sunset_result == ESP_OK && dusk_result == ESP_OK && commit_result == ESP_OK;
}
