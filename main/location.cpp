#include "location.hpp"

#include <cmath>
#include <cstdint>

#include "nvs.h"

namespace {
constexpr char kNamespace[] = "location";
constexpr char kLatitudeKey[] = "latitude";
constexpr char kLongitudeKey[] = "longitude";
constexpr double kScale = 1'000'000.0;

bool valid(const location_t &location)
{
    return std::isfinite(location.latitude) && std::isfinite(location.longitude) &&
        location.latitude >= -90.0 && location.latitude <= 90.0 &&
        location.longitude >= -180.0 && location.longitude <= 180.0;
}
}

bool location_load(location_t *location)
{
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
    int32_t latitude = 0;
    int32_t longitude = 0;
    const esp_err_t latitude_result = nvs_get_i32(handle, kLatitudeKey, &latitude);
    const esp_err_t longitude_result = nvs_get_i32(handle, kLongitudeKey, &longitude);
    nvs_close(handle);
    if (latitude_result != ESP_OK || longitude_result != ESP_OK) return false;
    *location = {latitude / kScale, longitude / kScale};
    return valid(*location);
}

bool location_save(const location_t &location)
{
    if (!valid(location)) return false;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t latitude_result = nvs_set_i32(handle, kLatitudeKey,
        static_cast<int32_t>(std::lround(location.latitude * kScale)));
    const esp_err_t longitude_result = nvs_set_i32(handle, kLongitudeKey,
        static_cast<int32_t>(std::lround(location.longitude * kScale)));
    const esp_err_t commit_result = nvs_commit(handle);
    nvs_close(handle);
    return latitude_result == ESP_OK && longitude_result == ESP_OK && commit_result == ESP_OK;
}
