#include "wifi.hpp"

#include <cstring>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "time_service.hpp"

namespace {
constexpr char kApSsid[] = "ChickenClock-Setup";
constexpr char kApPassword[] = "chickenclock";
constexpr char kHostName[] = "chicken";
constexpr char kNvsNamespace[] = "wifi";
constexpr char kSsidKey[] = "ssid";
constexpr char kPasswordKey[] = "password";
constexpr uint32_t kConnectionTimeoutMs = 10'000;
connection_state_t s_state = connection_state_t::kAccessPoint;
TickType_t s_station_start = 0;

struct credentials_t { char ssid[33] = {}; char password[65] = {}; };

template <size_t Size> void copy_string(uint8_t (&destination)[Size], const char *source) {
    std::strncpy(reinterpret_cast<char *>(destination), source, Size - 1);
    destination[Size - 1] = '\0';
}

bool load_credentials(credentials_t *credentials) {
    nvs_handle_t handle;
    const esp_err_t open_result = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (open_result == ESP_ERR_NVS_NOT_FOUND) return false;
    ESP_ERROR_CHECK(open_result);
    size_t ssid_size = sizeof(credentials->ssid);
    const esp_err_t ssid_result = nvs_get_str(handle, kSsidKey, credentials->ssid, &ssid_size);
    if (ssid_result == ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return false; }
    ESP_ERROR_CHECK(ssid_result);
    size_t password_size = sizeof(credentials->password);
    const esp_err_t password_result = nvs_get_str(handle, kPasswordKey, credentials->password, &password_size);
    if (password_result == ESP_ERR_NVS_NOT_FOUND) credentials->password[0] = '\0'; else ESP_ERROR_CHECK(password_result);
    nvs_close(handle);
    return credentials->ssid[0] != '\0';
}

void start_access_point() {
    s_state = connection_state_t::kAccessPoint;
    const esp_err_t stop_result = esp_wifi_stop();
    if (stop_result != ESP_OK && stop_result != ESP_ERR_WIFI_NOT_STARTED) ESP_ERROR_CHECK(stop_result);
    wifi_config_t config = {};
    copy_string(config.ap.ssid, kApSsid);
    copy_string(config.ap.password, kApPassword);
    config.ap.ssid_len = std::strlen(kApSsid);
    config.ap.channel = 1;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void start_station(const credentials_t &credentials) {
    wifi_config_t config = {};
    copy_string(config.sta.ssid, credentials.ssid);
    copy_string(config.sta.password, credentials.password);
    config.sta.threshold.authmode = credentials.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    s_state = connection_state_t::kConnecting;
    s_station_start = xTaskGetTickCount();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void event_handler(void *, esp_event_base_t base, int32_t id, void *) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED && s_state == connection_state_t::kConnecting)
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) { s_state = connection_state_t::kConnected; time_service_start(); }
}
}

void wifi_initialize()
{
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *station = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_netif_set_hostname(station, kHostName));
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(kHostName));
    ESP_ERROR_CHECK(mdns_instance_name_set("Chicken Clock"));
    ESP_ERROR_CHECK(mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0));
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr));
}

void wifi_start()
{
    credentials_t credentials;
    if (load_credentials(&credentials)) start_station(credentials); else start_access_point();
}

void wifi_update()
{
    if (s_state == connection_state_t::kConnecting &&
        xTaskGetTickCount() - s_station_start >= pdMS_TO_TICKS(kConnectionTimeoutMs)) start_access_point();
}

connection_state_t wifi_connection_state() { return s_state; }
const char *wifi_connection_state_name() {
    switch (s_state) {
    case connection_state_t::kConnecting: return "Connecting to Wi-Fi";
    case connection_state_t::kConnected: return "Connected to Wi-Fi";
    case connection_state_t::kAccessPoint: return "Setup access point";
    }
    return "Unknown";
}

bool wifi_scan(wifi_ap_record_t *networks, uint16_t *network_count) {
    return esp_wifi_scan_start(nullptr, true) == ESP_OK && esp_wifi_scan_get_ap_records(network_count, networks) == ESP_OK;
}

bool wifi_save_credentials(const char *ssid, const char *password) {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t ssid_result = nvs_set_str(handle, kSsidKey, ssid);
    const esp_err_t password_result = nvs_set_str(handle, kPasswordKey, password);
    const esp_err_t commit_result = nvs_commit(handle);
    nvs_close(handle);
    return ssid_result == ESP_OK && password_result == ESP_OK && commit_result == ESP_OK;
}
