#include "web_server.hpp"

#include <cstdint>
#include <cstring>

#include "esp_err.h"
#include "esp_http_server.h"
#include "time_service.hpp"
#include "wifi.hpp"

namespace {
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
constexpr size_t kMaxNetworks = 20;
constexpr size_t kMaxForm = 512;

void escaped(httpd_req_t *request, const char *value) {
    for (; *value; ++value) {
        const char *replacement = nullptr;
        if (*value == '&') replacement = "&amp;";
        else if (*value == '<') replacement = "&lt;";
        else if (*value == '>') replacement = "&gt;";
        else if (*value == '"') replacement = "&quot;";
        httpd_resp_send_chunk(request, replacement ? replacement : value, replacement ? HTTPD_RESP_USE_STRLEN : 1);
    }
}

esp_err_t root(httpd_req_t *request) {
    char utc[32] = {};
    const char *utc_text = time_service_format_utc(utc, sizeof(utc));
    httpd_resp_set_type(request, "text/html");
    httpd_resp_sendstr_chunk(request, "<!doctype html><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><link rel=\"icon\" href=\"/favicon.ico\"><title>Chicken Clock</title><h1>Chicken Clock</h1><p>Status: ");
    httpd_resp_sendstr_chunk(request, wifi_connection_state_name());
    httpd_resp_sendstr_chunk(request, "</p><p id=\"time-line\">--:--</p><script>const utc='");
    httpd_resp_sendstr_chunk(request, utc_text);
    httpd_resp_sendstr_chunk(request, "';const t=document.getElementById('time-line'),z=new Intl.DateTimeFormat(undefined,{timeZoneName:'short'}).formatToParts(new Date()).find(p=>p.type==='timeZoneName')?.value||'local';if(utc.endsWith('Z'))t.textContent=new Date(utc).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})+' ('+z+') ['+utc.replace('T',' ').replace('Z',' UTC')+']';else t.textContent='--:-- ('+z+') [Waiting for NTP synchronization]';</script><p><a href=\"/wifi\">Set Wi-Fi</a></p>");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t favicon(httpd_req_t *request) {
    httpd_resp_set_type(request, "image/x-icon");
    return httpd_resp_send(request, reinterpret_cast<const char *>(favicon_ico_start), favicon_ico_end - favicon_ico_start);
}

bool decode(char *value) {
    auto hex = [](char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
    char *write = value;
    for (char *read = value; *read; ++read) {
        if (*read == '+') *write++ = ' ';
        else if (*read == '%') { if (!read[1] || !read[2]) return false; const int high = hex(*++read), low = hex(*++read); if (high < 0 || low < 0) return false; *write++ = static_cast<char>((high << 4) | low); }
        else *write++ = *read;
    }
    *write = '\0'; return true;
}

bool parse_credentials(char *form, char *ssid, size_t ssid_size, char *password, size_t password_size) {
    bool has_ssid = false, has_password = false;
    for (char *field = form; field;) {
        char *next = std::strchr(field, '&'); if (next) *next = '\0';
        if (std::strncmp(field, "ssid=", 5) == 0) { char *value = field + 5; if (!decode(value) || std::strlen(value) >= ssid_size) return false; std::strcpy(ssid, value); has_ssid = true; }
        else if (std::strncmp(field, "password=", 9) == 0) { char *value = field + 9; if (!decode(value) || std::strlen(value) >= password_size) return false; std::strcpy(password, value); has_password = true; }
        field = next ? next + 1 : nullptr;
    }
    return has_ssid && has_password;
}

esp_err_t wifi_get(httpd_req_t *request) {
    wifi_ap_record_t networks[kMaxNetworks] = {}; uint16_t count = kMaxNetworks;
    if (!wifi_scan(networks, &count)) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed");
    httpd_resp_set_type(request, "text/html");
    httpd_resp_sendstr_chunk(request, "<!doctype html><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><link rel=\"icon\" href=\"/favicon.ico\"><title>Set Wi-Fi</title><h1>Set Wi-Fi</h1><form method=\"post\" action=\"/wifi\"><p><label>Network <select name=\"ssid\" required><option value=\"\">Choose a network</option>");
    for (uint16_t i = 0; i < count; ++i) { httpd_resp_sendstr_chunk(request, "<option value=\""); escaped(request, reinterpret_cast<const char *>(networks[i].ssid)); httpd_resp_sendstr_chunk(request, "\">"); escaped(request, reinterpret_cast<const char *>(networks[i].ssid)); httpd_resp_sendstr_chunk(request, "</option>"); }
    httpd_resp_sendstr_chunk(request, "</select></label></p><p><label>Passphrase <input name=\"password\" type=\"password\" maxlength=\"63\"></label></p><button type=\"submit\">Save Wi-Fi settings</button></form><p><a href=\"/\">Back</a></p>");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t wifi_post(httpd_req_t *request) {
    if (request->content_len == 0 || request->content_len >= kMaxForm) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    char form[kMaxForm] = {}; size_t received = 0;
    while (received < request->content_len) { const int read = httpd_req_recv(request, form + received, request->content_len - received); if (read <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not read Wi-Fi settings"); received += read; }
    char ssid[33] = {}, password[65] = {}; form[received] = '\0';
    if (!parse_credentials(form, ssid, sizeof(ssid), password, sizeof(password)) || !ssid[0]) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    if (!wifi_save_credentials(ssid, password)) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save Wi-Fi settings");
    return httpd_resp_send(request, "<!doctype html><title>Wi-Fi saved</title><h1>Wi-Fi settings saved</h1><p>Restart the device to connect using the new settings.</p><p><a href=\"/\">Back</a></p>", HTTPD_RESP_USE_STRLEN);
}
}

void web_server_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); httpd_handle_t server = nullptr; ESP_ERROR_CHECK(httpd_start(&server, &config));
    httpd_uri_t root_uri = {}; root_uri.uri = "/"; root_uri.method = HTTP_GET; root_uri.handler = root; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
    httpd_uri_t icon_uri = {}; icon_uri.uri = "/favicon.ico"; icon_uri.method = HTTP_GET; icon_uri.handler = favicon; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &icon_uri));
    httpd_uri_t get_uri = {}; get_uri.uri = "/wifi"; get_uri.method = HTTP_GET; get_uri.handler = wifi_get; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));
    httpd_uri_t post_uri = {}; post_uri.uri = "/wifi"; post_uri.method = HTTP_POST; post_uri.handler = wifi_post; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &post_uri));
}
