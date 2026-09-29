#include "sun_schedule.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.hpp"
#include "location.hpp"
#include "sun_settings.hpp"
#include "wifi.hpp"

namespace {
constexpr size_t kResponseCapacity = 4096;
constexpr int64_t kValidTimeEpoch = 1'700'000'000;
constexpr TickType_t kWifiSettlingDelay = pdMS_TO_TICKS(10 * 1000);
constexpr TickType_t kFirstRetryDelay = pdMS_TO_TICKS(10 * 1000);
constexpr TickType_t kRetryDelay = pdMS_TO_TICKS(5 * 60 * 1000);

struct sun_events_t { time_t sunset = 0; time_t dusk = 0; };

char s_response[kResponseCapacity] = {};
size_t s_response_length = 0;
sun_events_t s_today = {};
sun_events_t s_tomorrow = {};
bool s_fetch_failed = false;
bool s_first_attempt_complete = false;
TickType_t s_connected_since = 0;
TickType_t s_next_fetch_attempt = 0;

esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && !event->user_data &&
        s_response_length + event->data_len < kResponseCapacity) {
        std::memcpy(s_response + s_response_length, event->data, event->data_len);
        s_response_length += event->data_len;
    }
    return ESP_OK;
}

bool json_time(const char *name, time_t *value)
{
    const char *key = std::strstr(s_response, name);
    if (key == nullptr) return false;
    const char *colon = std::strchr(key, ':');
    if (colon == nullptr) return false;
    char *end = nullptr;
    const int64_t parsed = std::strtoll(colon + 1, &end, 10);
    if (end == colon + 1 || parsed < kValidTimeEpoch) return false;
    *value = static_cast<time_t>(parsed);
    return true;
}

bool fetch_events(const location_t &location, const char *date, sun_events_t *events)
{
    char url[192] = {};
    std::snprintf(url, sizeof(url),
        "http://api.sunrise-sunset.org/v2?lat=%.6f&lng=%.6f&date=%s&time_format=unix",
        location.latitude, location.longitude, date);
    s_response_length = 0;
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 10'000;
    config.event_handler = http_event;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) return false;
    const esp_err_t result = esp_http_client_perform(client);
    const bool success = result == ESP_OK && esp_http_client_get_status_code(client) == 200 &&
        s_response_length < kResponseCapacity;
    esp_http_client_cleanup(client);
    if (!success) return false;
    s_response[s_response_length] = '\0';
    return json_time("\"sunset\"", &events->sunset) && json_time("\"dusk\"", &events->dusk);
}

bool fetch_today_and_tomorrow(const location_t &location)
{
    sun_events_t today = {};
    sun_events_t tomorrow = {};
    if (!fetch_events(location, "today", &today) || !fetch_events(location, "tomorrow", &tomorrow)) return false;
    s_today = today;
    s_tomorrow = tomorrow;
    return true;
}

bool fetch_next_tomorrow(const location_t &location)
{
    sun_events_t tomorrow = {};
    if (!fetch_events(location, "tomorrow", &tomorrow)) return false;
    s_today = s_tomorrow;
    s_tomorrow = tomorrow;
    return true;
}

bool time_to_fetch(TickType_t ticks)
{
    return static_cast<int32_t>(ticks - s_next_fetch_attempt) >= 0;
}
}

void sun_schedule_update()
{
    const TickType_t ticks = xTaskGetTickCount();
    if (wifi_connection_state() != connection_state_t::kConnected) {
        s_connected_since = 0;
        return;
    }
    if (s_connected_since == 0) s_connected_since = ticks;
    if (static_cast<int32_t>(ticks - s_connected_since) < static_cast<int32_t>(kWifiSettlingDelay)) return;

    const time_t now = std::time(nullptr);
    if (now < kValidTimeEpoch) return;
    location_t location = {};
    if (!location_load(&location)) return;
    sun_settings_t settings = {0, 0};
    sun_settings_load(&settings);

    const bool need_initial_events = s_today.sunset == 0 || s_tomorrow.sunset == 0;
    const bool need_next_day = !need_initial_events &&
        now > s_tomorrow.dusk + settings.dusk_offset_minutes * 60;
    if ((need_initial_events || need_next_day) && time_to_fetch(ticks)) {
        const bool fetched = need_initial_events ? fetch_today_and_tomorrow(location) : fetch_next_tomorrow(location);
        if (fetched) {
            s_fetch_failed = false;
            s_next_fetch_attempt = 0;
        } else {
            s_fetch_failed = true;
            s_next_fetch_attempt = ticks + (s_first_attempt_complete ? kRetryDelay : kFirstRetryDelay);
        }
        s_first_attempt_complete = true;
    }

    const bool active = s_today.sunset != 0 && now >= s_today.sunset + settings.sunset_offset_minutes * 60 &&
        now < s_today.dusk + settings.dusk_offset_minutes * 60;
    led_set_mosfet1(active);
}

sun_schedule_status_t sun_schedule_status()
{
    sun_settings_t settings = {0, 0};
    sun_settings_load(&settings);
    const time_t now = std::time(nullptr);
    const time_t sunset = s_today.sunset + settings.sunset_offset_minutes * 60;
    const time_t dusk = s_today.dusk + settings.dusk_offset_minutes * 60;
    const time_t tomorrow_sunset = s_tomorrow.sunset + settings.sunset_offset_minutes * 60;
    if (wifi_connection_state() != connection_state_t::kConnected) return {false, false, "Waiting for Wi-Fi", 0};
    if (now < kValidTimeEpoch) return {false, false, "Waiting for NTP", 0};
    location_t location = {};
    if (!location_load(&location)) return {false, false, "Set a location", 0};
    if (s_today.sunset == 0 || s_tomorrow.sunset == 0) return {false, false, s_fetch_failed ? "Sun data retrying" : "Loading sun data", 0};
    if (now < sunset) return {true, false, "on", static_cast<int>((sunset - now + 30) / 60)};
    if (now < dusk) return {true, true, "off", static_cast<int>((dusk - now + 30) / 60)};
    return {true, false, "on", static_cast<int>((tomorrow_sunset - now + 30) / 60)};
}
