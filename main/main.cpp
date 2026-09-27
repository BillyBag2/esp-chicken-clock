#include "board_pins.hpp"

#include <cstdint>
#include <cstring>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace {

extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

constexpr uint32_t kLedUpdateIntervalMs = 20;
constexpr uint32_t kStationConnectionTimeoutMs = 10'000;
constexpr uint32_t kConnectingLedPeriodMs = 1'000;
constexpr uint32_t kAccessPointLedPeriodMs = 200;
constexpr uint32_t kConnectedLedPeriodMs = 1'000;
constexpr uint32_t kConnectedLedBlipMs = 100;

constexpr char kAccessPointSsid[] = "ChickenClock-Setup";
constexpr char kAccessPointPassword[] = "chickenclock";
constexpr char kHostName[] = "chicken";
constexpr char kWifiNvsNamespace[] = "wifi";
constexpr char kWifiSsidNvsKey[] = "ssid";
constexpr char kWifiPasswordNvsKey[] = "password";
constexpr size_t kMaximumWifiNetworks = 20;
constexpr size_t kMaximumWifiFormSize = 512;

enum class connection_state_t {
    kConnecting,
    kConnected,
    kAccessPoint,
};

volatile connection_state_t s_connection_state = connection_state_t::kAccessPoint;

struct wifi_credentials_t {
    char ssid[33] = {};
    char password[65] = {};
};

const char *connection_state_name()
{
    switch (s_connection_state) {
    case connection_state_t::kConnecting:
        return "Connecting to Wi-Fi";
    case connection_state_t::kConnected:
        return "Connected to Wi-Fi";
    case connection_state_t::kAccessPoint:
        return "Setup access point";
    }

    return "Unknown";
}

void send_html_escaped(httpd_req_t *request, const char *value)
{
    for (const char *character = value; *character != '\0'; ++character) {
        switch (*character) {
        case '&':
            httpd_resp_send_chunk(request, "&amp;", HTTPD_RESP_USE_STRLEN);
            break;
        case '<':
            httpd_resp_send_chunk(request, "&lt;", HTTPD_RESP_USE_STRLEN);
            break;
        case '>':
            httpd_resp_send_chunk(request, "&gt;", HTTPD_RESP_USE_STRLEN);
            break;
        case '"':
            httpd_resp_send_chunk(request, "&quot;", HTTPD_RESP_USE_STRLEN);
            break;
        default:
            httpd_resp_send_chunk(request, character, 1);
            break;
        }
    }
}

esp_err_t root_get_handler(httpd_req_t *request)
{
    static constexpr char kPage[] =
        "<!doctype html>\n"
        "<html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<link rel=\"icon\" href=\"/favicon.ico\">"
        "<title>Chicken Clock</title></head><body>"
        "<h1>Chicken Clock</h1><p>Status: ";

    httpd_resp_set_type(request, "text/html");
    httpd_resp_send_chunk(request, kPage, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(request, connection_state_name(), HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(request,
        "</p><p><a href=\"/wifi\">Set Wi-Fi</a></p></body></html>\n", HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t favicon_get_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "image/x-icon");
    return httpd_resp_send(request, reinterpret_cast<const char *>(favicon_ico_start),
        favicon_ico_end - favicon_ico_start);
}

bool decode_url_component(char *value)
{
    auto hex_value = [](char character) -> int {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        return -1;
    };

    char *write = value;
    for (char *read = value; *read != '\0'; ++read) {
        if (*read == '+') {
            *write++ = ' ';
        } else if (*read == '%') {
            if (read[1] == '\0' || read[2] == '\0') {
                return false;
            }
            const int high_nibble = hex_value(*(++read));
            const int low_nibble = hex_value(*(++read));
            if (high_nibble < 0 || low_nibble < 0) {
                return false;
            }
            *write++ = static_cast<char>((high_nibble << 4) | low_nibble);
        } else {
            *write++ = *read;
        }
    }
    *write = '\0';
    return true;
}

bool parse_wifi_form(char *form, wifi_credentials_t *credentials)
{
    bool has_ssid = false;
    bool has_password = false;
    for (char *field = form; field != nullptr;) {
        char *next_field = std::strchr(field, '&');
        if (next_field != nullptr) {
            *next_field = '\0';
        }

        if (std::strncmp(field, "ssid=", 5) == 0) {
            char *value = field + 5;
            if (!decode_url_component(value) || std::strlen(value) >= sizeof(credentials->ssid)) {
                return false;
            }
            std::strcpy(credentials->ssid, value);
            has_ssid = true;
        } else if (std::strncmp(field, "password=", 9) == 0) {
            char *value = field + 9;
            if (!decode_url_component(value) || std::strlen(value) >= sizeof(credentials->password)) {
                return false;
            }
            std::strcpy(credentials->password, value);
            has_password = true;
        }
        field = next_field == nullptr ? nullptr : next_field + 1;
    }
    return has_ssid && has_password;
}

esp_err_t wifi_get_handler(httpd_req_t *request)
{
    wifi_ap_record_t networks[kMaximumWifiNetworks] = {};
    uint16_t network_count = kMaximumWifiNetworks;
    const esp_err_t scan_result = esp_wifi_scan_start(nullptr, true);
    if (scan_result != ESP_OK || esp_wifi_scan_get_ap_records(&network_count, networks) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed");
    }

    httpd_resp_set_type(request, "text/html");
    httpd_resp_sendstr_chunk(request,
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<link rel=\"icon\" href=\"/favicon.ico\">"
        "<title>Set Wi-Fi</title></head><body><h1>Set Wi-Fi</h1>"
        "<form method=\"post\" action=\"/wifi\"><p><label>Network "
        "<select name=\"ssid\" required><option value=\"\">Choose a network</option>");
    for (uint16_t index = 0; index < network_count; ++index) {
        httpd_resp_sendstr_chunk(request, "<option value=\"");
        send_html_escaped(request, reinterpret_cast<const char *>(networks[index].ssid));
        httpd_resp_sendstr_chunk(request, "\">");
        send_html_escaped(request, reinterpret_cast<const char *>(networks[index].ssid));
        httpd_resp_sendstr_chunk(request, "</option>");
    }
    httpd_resp_sendstr_chunk(request,
        "</select></label></p><p><label>Passphrase "
        "<input name=\"password\" type=\"password\" maxlength=\"63\" autocomplete=\"current-password\"></label></p>"
        "<p><button type=\"submit\">Save Wi-Fi settings</button></p></form>"
        "<p><a href=\"/\">Back</a></p></body></html>");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

bool save_wifi_credentials(const wifi_credentials_t &credentials)
{
    nvs_handle_t handle;
    const esp_err_t open_result = nvs_open(kWifiNvsNamespace, NVS_READWRITE, &handle);
    if (open_result != ESP_OK) {
        return false;
    }

    const esp_err_t ssid_result = nvs_set_str(handle, kWifiSsidNvsKey, credentials.ssid);
    const esp_err_t password_result = nvs_set_str(handle, kWifiPasswordNvsKey, credentials.password);
    const esp_err_t commit_result = nvs_commit(handle);
    nvs_close(handle);
    return ssid_result == ESP_OK && password_result == ESP_OK && commit_result == ESP_OK;
}

esp_err_t wifi_post_handler(httpd_req_t *request)
{
    if (request->content_len == 0 || request->content_len >= kMaximumWifiFormSize) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    }

    char form[kMaximumWifiFormSize] = {};
    size_t received = 0;
    while (received < request->content_len) {
        const int read = httpd_req_recv(request, form + received, request->content_len - received);
        if (read <= 0) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not read Wi-Fi settings");
        }
        received += read;
    }
    form[received] = '\0';

    wifi_credentials_t credentials;
    if (!parse_wifi_form(form, &credentials) || credentials.ssid[0] == '\0') {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    }
    if (!save_wifi_credentials(credentials)) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save Wi-Fi settings");
    }

    httpd_resp_set_type(request, "text/html");
    return httpd_resp_sendstr(request,
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<link rel=\"icon\" href=\"/favicon.ico\">"
        "<title>Wi-Fi saved</title></head><body><h1>Wi-Fi settings saved</h1>"
        "<p>Restart the device to connect using the new settings.</p><p><a href=\"/\">Back</a></p>"
        "</body></html>");
}

void start_web_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root_uri = {};
    root_uri.uri = "/";
    root_uri.method = HTTP_GET;
    root_uri.handler = root_get_handler;
    root_uri.user_ctx = nullptr;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));

    httpd_uri_t favicon_uri = {};
    favicon_uri.uri = "/favicon.ico";
    favicon_uri.method = HTTP_GET;
    favicon_uri.handler = favicon_get_handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &favicon_uri));

    httpd_uri_t wifi_get_uri = {};
    wifi_get_uri.uri = "/wifi";
    wifi_get_uri.method = HTTP_GET;
    wifi_get_uri.handler = wifi_get_handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_get_uri));

    httpd_uri_t wifi_post_uri = {};
    wifi_post_uri.uri = "/wifi";
    wifi_post_uri.method = HTTP_POST;
    wifi_post_uri.handler = wifi_post_handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_post_uri));
}

void start_mdns()
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(kHostName));
    ESP_ERROR_CHECK(mdns_instance_name_set("Chicken Clock"));
    ESP_ERROR_CHECK(mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0));
}

void configure_output(gpio_num_t pin)
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
}

void configure_button()
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << board_pins::kIo0Button;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
}

template <size_t DestinationSize>
void copy_string(uint8_t (&destination)[DestinationSize], const char *source)
{
    std::strncpy(reinterpret_cast<char *>(destination), source, DestinationSize - 1);
    destination[DestinationSize - 1] = '\0';
}

void start_access_point()
{
    s_connection_state = connection_state_t::kAccessPoint;

    const esp_err_t stop_result = esp_wifi_stop();
    if (stop_result != ESP_OK && stop_result != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_ERROR_CHECK(stop_result);
    }

    wifi_config_t access_point_config = {};
    copy_string(access_point_config.ap.ssid, kAccessPointSsid);
    copy_string(access_point_config.ap.password, kAccessPointPassword);
    access_point_config.ap.ssid_len = static_cast<uint8_t>(std::strlen(kAccessPointSsid));
    access_point_config.ap.channel = 1;
    access_point_config.ap.max_connection = 4;
    access_point_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    // AP+station mode keeps the setup site reachable while it scans networks.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &access_point_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool load_wifi_credentials(wifi_credentials_t *credentials)
{
    nvs_handle_t handle;
    const esp_err_t open_result = nvs_open(kWifiNvsNamespace, NVS_READONLY, &handle);
    if (open_result == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    ESP_ERROR_CHECK(open_result);

    size_t ssid_size = sizeof(credentials->ssid);
    const esp_err_t ssid_result = nvs_get_str(handle, kWifiSsidNvsKey, credentials->ssid, &ssid_size);
    if (ssid_result == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return false;
    }
    ESP_ERROR_CHECK(ssid_result);

    size_t password_size = sizeof(credentials->password);
    const esp_err_t password_result = nvs_get_str(handle, kWifiPasswordNvsKey, credentials->password, &password_size);
    if (password_result == ESP_ERR_NVS_NOT_FOUND) {
        credentials->password[0] = '\0';
    } else {
        ESP_ERROR_CHECK(password_result);
    }
    nvs_close(handle);

    return credentials->ssid[0] != '\0';
}

void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED &&
        s_connection_state == connection_state_t::kConnecting) {
        // Retry until the main task reaches its 10-second connection deadline.
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_connection_state = connection_state_t::kConnected;
    }
}

void start_station(const wifi_credentials_t &credentials)
{
    wifi_config_t station_config = {};
    copy_string(station_config.sta.ssid, credentials.ssid);
    copy_string(station_config.sta.password, credentials.password);
    station_config.sta.threshold.authmode = credentials.password[0] == '\0'
        ? WIFI_AUTH_OPEN
        : WIFI_AUTH_WPA2_PSK;

    s_connection_state = connection_state_t::kConnecting;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

bool led_should_be_on(uint32_t elapsed_ms)
{
    if (gpio_get_level(board_pins::kIo0Button) == 0) {
        return false;
    }

    switch (s_connection_state) {
    case connection_state_t::kConnecting:
        return elapsed_ms % kConnectingLedPeriodMs < kConnectingLedPeriodMs / 2;
    case connection_state_t::kConnected:
        return elapsed_ms % kConnectedLedPeriodMs < kConnectedLedBlipMs;
    case connection_state_t::kAccessPoint:
        return elapsed_ms % kAccessPointLedPeriodMs < kAccessPointLedPeriodMs / 2;
    }

    return false;
}

}  // namespace

extern "C" void app_main(void)
{
    configure_output(board_pins::kLed);
    configure_output(board_pins::kMosfet1);
    configure_output(board_pins::kMosfet2);
    configure_button();

    // Leave loads disconnected until their control behaviour is implemented.
    ESP_ERROR_CHECK(gpio_set_level(board_pins::kMosfet1, 0));
    ESP_ERROR_CHECK(gpio_set_level(board_pins::kMosfet2, 0));

    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *station_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_netif_set_hostname(station_netif, kHostName));
    start_mdns();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    wifi_credentials_t credentials;
    if (load_wifi_credentials(&credentials)) {
        start_station(credentials);
    } else {
        start_access_point();
    }
    start_web_server();

    const TickType_t station_start_time = xTaskGetTickCount();
    while (true) {
        if (s_connection_state == connection_state_t::kConnecting &&
            xTaskGetTickCount() - station_start_time >= pdMS_TO_TICKS(kStationConnectionTimeoutMs)) {
            start_access_point();
        }

        const uint32_t elapsed_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_ERROR_CHECK(gpio_set_level(board_pins::kLed, led_should_be_on(elapsed_ms) ? 1 : 0));
        vTaskDelay(pdMS_TO_TICKS(kLedUpdateIntervalMs));
    }
}
