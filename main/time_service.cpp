#include "time_service.hpp"

#include <ctime>

#include "esp_err.h"
#include "esp_netif_sntp.h"

namespace {
constexpr char kServers[][24] = {"time.cloudflare.com", "0.pool.ntp.org", "1.pool.ntp.org"};
bool s_started = false;
}

void time_service_start()
{
    if (s_started) return;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,
        ESP_SNTP_SERVER_LIST(kServers[0], kServers[1], kServers[2]));
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));
    s_started = true;
}

const char *time_service_format_utc(char *buffer, size_t buffer_size)
{
    const time_t now = std::time(nullptr);
    if (now < 1'700'000'000) return "Waiting for NTP synchronization";
    tm utc = {};
    gmtime_r(&now, &utc);
    std::strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}
