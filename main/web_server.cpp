#include "web_server.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_err.h"
#include "esp_http_server.h"
#include "control_mode.hpp"
#include "location.hpp"
#include "sun_settings.hpp"
#include "sun_schedule.hpp"
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
    wifi_note_activity();
    char utc[32] = {};
    const char *utc_text = time_service_format_utc(utc, sizeof(utc));
    httpd_resp_set_type(request, "text/html");
    httpd_resp_sendstr_chunk(request, "<!doctype html><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><link rel=\"icon\" href=\"/favicon.ico\"><title>Chicken Clock</title><nav><a href=\"/\">Home</a> | <a href=\"/settings\">Settings</a> | <a href=\"/wifi\">Set Wi-Fi</a> | <span id=\"sun-indicator\">Loading...</span></nav><h1>Chicken Clock</h1><p><button type=\"button\" onclick=\"setMode('timer')\">Timer</button> <button type=\"button\" onclick=\"setMode('on')\">On</button> <button type=\"button\" onclick=\"setMode('off')\">Off</button></p><script>const utc='");
    httpd_resp_sendstr_chunk(request, utc_text);
    httpd_resp_sendstr_chunk(request, "';const now=utc.endsWith('Z')?new Date(utc):new Date(),clock=now.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'}),indicator=s=>{let text='Mode: '+s.mode+' Time: '+clock+' State: '+(s.mosfet1_on?'ON':'OFF');if(s.mode==='TIMER'){if(!s.available)return text+'; '+s.next_event;const h=Math.floor(s.minutes/60),d=h?h+'h '+s.minutes%60+'min':s.minutes+'min',at=new Date(s.next_event_unix*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});text+='; '+s.next_event+' in '+d+' @ '+at;}return text;},setMode=mode=>fetch('/mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'mode='+mode}).then(r=>{if(r.ok)location.reload();});fetch('/sun-status').then(r=>r.json()).then(s=>document.getElementById('sun-indicator').textContent=indicator(s));</script>");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t settings_get(httpd_req_t *request)
{
    wifi_note_activity();
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_sendstr(request,
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<link rel=\"icon\" href=\"/favicon.ico\">"
        "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\">"
        "<title>Settings</title></head><body>"
        "<nav><a href=\"/\">Home</a> | <a href=\"/settings\">Settings</a> | "
        "<a href=\"/wifi\">Set Wi-Fi</a> | <span id=\"sun-indicator\">Sun events loading...</span></nav><h1>Settings</h1>"
        "<h2>Sun event offsets</h2><p><label>Sunset offset (minutes) <input id=\"sunset-offset\" type=\"number\" min=\"-128\" max=\"127\" value=\"0\"></label></p>"
        "<p><label>Dusk offset (minutes) <input id=\"dusk-offset\" type=\"number\" min=\"-128\" max=\"127\" value=\"0\"></label> <button id=\"save-offsets\" type=\"button\">Save offsets</button></p><p id=\"offset-status\"></p>"
        "<h2>Location</h2><p>Click the map to choose the device location. Drag the marker to refine it.</p>"
        "<button id=\"recenter\" type=\"button\" disabled>Recenter on pin</button>"
        "<p id=\"location\">Location has not been selected.</p>"
        "<div id=\"map\" style=\"height:360px;border:1px solid #777\"></div>"
        "<h2>Sun events</h2><table border=\"1\"><thead><tr><th>Date</th><th>Sunrise</th><th>Sunset</th><th>Dusk</th></tr></thead><tbody id=\"sun-events\"><tr><td colspan=\"4\">Select a location to load events.</td></tr></tbody></table><p>Sun API URL: <a id=\"sun-url\" href=\"#\">Not requested yet</a></p><p><a href=\"https://sunrise-sunset.org\">Sunrise-Sunset.org</a></p>"
        "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></script>"
        "<script>const message=document.getElementById('location'),recenter=document.getElementById('recenter'),map=L.map('map').setView([20,0],2);"
        "L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap contributors</a>'}).addTo(map);"
        "const rows=document.getElementById('sun-events');function eventTime(value){return value?new Date(value*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'}):'-';}function loadSun(point){const start=new Date(),end=new Date(start);end.setDate(start.getDate()+6);const date=value=>value.toISOString().slice(0,10),url='https://api.sunrise-sunset.org/v2?lat='+point.lat+'&lng='+point.lng+'&date_start='+date(start)+'&date_end='+date(end)+'&time_format=unix',link=document.getElementById('sun-url');link.href=url;link.textContent=url;rows.innerHTML='<tr><td colspan=\"4\">Loading…</td></tr>';fetch(url).then(response=>response.json()).then(data=>{rows.innerHTML='';(data.days||[]).forEach(day=>{const row=document.createElement('tr');row.innerHTML='<td>'+day.date+'</td><td>'+eventTime(day.sunrise)+'</td><td>'+eventTime(day.sunset)+'</td><td>'+eventTime(day.dusk)+'</td>';rows.appendChild(row);});}).catch(()=>rows.innerHTML='<tr><td colspan=\"4\">Could not load sun events.</td></tr>');}"
        "let marker;function choose(point,persist=true){if(!marker){marker=L.marker(point,{draggable:true}).addTo(map);marker.on('dragend',()=>choose(marker.getLatLng()));}else marker.setLatLng(point);recenter.disabled=false;message.textContent='Latitude: '+point.lat.toFixed(6)+'; Longitude: '+point.lng.toFixed(6);loadSun(point);if(persist)fetch('/location',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'latitude='+point.lat+'&longitude='+point.lng}).then(response=>{if(!response.ok)message.textContent+=' (could not save)';}).catch(()=>message.textContent+=' (could not save)');}"
        "recenter.addEventListener('click',()=>{if(marker)map.setView(marker.getLatLng(),13);});"
        "map.on('click',event=>choose(event.latlng));fetch('/location').then(response=>response.ok?response.json():null).then(saved=>{if(saved){const point={lat:saved.latitude,lng:saved.longitude};map.setView(point,13);choose(point,false);}});fetch('/sun-settings').then(response=>response.ok?response.json():null).then(saved=>{if(saved){document.getElementById('sunset-offset').value=saved.sunset_offset;document.getElementById('dusk-offset').value=saved.dusk_offset;}});document.getElementById('save-offsets').addEventListener('click',()=>{const sunset=document.getElementById('sunset-offset').value,dusk=document.getElementById('dusk-offset').value;fetch('/sun-settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'sunset='+sunset+'&dusk='+dusk}).then(response=>document.getElementById('offset-status').textContent=response.ok?'Offsets saved.':'Could not save offsets.');});fetch('/sun-status').then(r=>r.json()).then(s=>{const clock=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});let text='Mode: '+s.mode+' Time: '+clock+' State: '+(s.mosfet1_on?'ON':'OFF');if(s.mode==='TIMER'){if(s.available){const h=Math.floor(s.minutes/60),d=h?h+'h '+s.minutes%60+'min':s.minutes+'min',at=new Date(s.next_event_unix*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});text+='; '+s.next_event+' in '+d+' @ '+at;}else text+='; '+s.next_event;}document.getElementById('sun-indicator').textContent=text;});</script>"
        "</body></html>");
}

esp_err_t location_get(httpd_req_t *request)
{
    wifi_note_activity();
    location_t location = {};
    if (!location_load(&location)) return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Location not set");
    char response[80] = {};
    std::snprintf(response, sizeof(response), "{\"latitude\":%.6f,\"longitude\":%.6f}",
        location.latitude, location.longitude);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, response);
}

esp_err_t location_post(httpd_req_t *request)
{
    wifi_note_activity();
    if (request->content_len == 0 || request->content_len >= 96) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid location");
    }
    char form[96] = {};
    size_t received = 0;
    while (received < request->content_len) {
        const int read = httpd_req_recv(request, form + received, request->content_len - received);
        if (read <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not read location");
        received += read;
    }
    location_t location = {};
    form[received] = '\0';
    if (std::sscanf(form, "latitude=%lf&longitude=%lf", &location.latitude, &location.longitude) != 2 ||
        !location_save(location)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid location");
    return httpd_resp_send(request, nullptr, 0);
}

esp_err_t sun_settings_get(httpd_req_t *request)
{
    wifi_note_activity();
    sun_settings_t settings = {0, 0};
    sun_settings_load(&settings);
    char response[64] = {};
    std::snprintf(response, sizeof(response), "{\"sunset_offset\":%d,\"dusk_offset\":%d}",
        settings.sunset_offset_minutes, settings.dusk_offset_minutes);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, response);
}

esp_err_t sun_settings_post(httpd_req_t *request)
{
    wifi_note_activity();
    if (request->content_len == 0 || request->content_len >= 48) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid offsets");
    }
    char form[48] = {};
    size_t received = 0;
    while (received < request->content_len) {
        const int read = httpd_req_recv(request, form + received, request->content_len - received);
        if (read <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not read offsets");
        received += read;
    }
    int sunset = 0;
    int dusk = 0;
    form[received] = '\0';
    if (std::sscanf(form, "sunset=%d&dusk=%d", &sunset, &dusk) != 2 ||
        sunset < -128 || sunset > 127 || dusk < -128 || dusk > 127) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid offsets");
    }
    const sun_settings_t settings = {static_cast<int8_t>(sunset), static_cast<int8_t>(dusk)};
    if (!sun_settings_save(settings)) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save offsets");
    return httpd_resp_send(request, nullptr, 0);
}

esp_err_t sun_status_get(httpd_req_t *request)
{
    wifi_note_activity();
    const sun_schedule_status_t status = sun_schedule_status();
    char response[224] = {};
    std::snprintf(response, sizeof(response),
        "{\"mode\":\"%s\",\"available\":%s,\"mosfet1_on\":%s,\"next_event\":\"%s\",\"minutes\":%d,\"next_event_unix\":%lld}",
        status.mode, status.available ? "true" : "false", status.mosfet1_on ? "true" : "false",
        status.next_event, status.minutes_to_next_event, status.next_event_unix);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, response);
}

esp_err_t mode_post(httpd_req_t *request)
{
    wifi_note_activity();
    if (request->content_len == 0 || request->content_len >= 16) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid mode");
    }
    char form[16] = {};
    size_t received = 0;
    while (received < request->content_len) {
        const int read = httpd_req_recv(request, form + received, request->content_len - received);
        if (read <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not read mode");
        received += read;
    }
    control_mode_t mode = control_mode_t::kTimer;
    if (std::strcmp(form, "mode=timer") == 0) mode = control_mode_t::kTimer;
    else if (std::strcmp(form, "mode=on") == 0) mode = control_mode_t::kOn;
    else if (std::strcmp(form, "mode=off") == 0) mode = control_mode_t::kOff;
    else return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid mode");
    if (!control_mode_save(mode)) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save mode");
    return httpd_resp_send(request, nullptr, 0);
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
    wifi_note_activity();
    wifi_ap_record_t networks[kMaxNetworks] = {}; uint16_t count = kMaxNetworks;
    if (!wifi_scan(networks, &count)) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed");
    httpd_resp_set_type(request, "text/html");
    httpd_resp_sendstr_chunk(request, "<!doctype html><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><link rel=\"icon\" href=\"/favicon.ico\"><title>Set Wi-Fi</title><nav><a href=\"/\">Home</a> | <a href=\"/settings\">Settings</a> | <a href=\"/wifi\">Set Wi-Fi</a> | <span id=\"sun-indicator\">Sun events loading...</span></nav><h1>Set Wi-Fi</h1><form method=\"post\" action=\"/wifi\"><p><label>Network <select name=\"ssid\" required><option value=\"\">Choose a network</option>");
    for (uint16_t i = 0; i < count; ++i) { httpd_resp_sendstr_chunk(request, "<option value=\""); escaped(request, reinterpret_cast<const char *>(networks[i].ssid)); httpd_resp_sendstr_chunk(request, "\">"); escaped(request, reinterpret_cast<const char *>(networks[i].ssid)); httpd_resp_sendstr_chunk(request, "</option>"); }
    httpd_resp_sendstr_chunk(request, "</select></label></p><p><label>Passphrase <input name=\"password\" type=\"password\" maxlength=\"63\"></label></p><button type=\"submit\">Save Wi-Fi settings</button></form><script>fetch('/sun-status').then(r=>r.json()).then(s=>{const clock=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});let text='Mode: '+s.mode+' Time: '+clock+' State: '+(s.mosfet1_on?'ON':'OFF');if(s.mode==='TIMER'){if(s.available){const h=Math.floor(s.minutes/60),d=h?h+'h '+s.minutes%60+'min':s.minutes+'min',at=new Date(s.next_event_unix*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});text+='; '+s.next_event+' in '+d+' @ '+at;}else text+='; '+s.next_event;}document.getElementById('sun-indicator').textContent=text;});</script>");
    return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t wifi_post(httpd_req_t *request) {
    wifi_note_activity();
    if (request->content_len == 0 || request->content_len >= kMaxForm) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    char form[kMaxForm] = {}; size_t received = 0;
    while (received < request->content_len) { const int read = httpd_req_recv(request, form + received, request->content_len - received); if (read <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not read Wi-Fi settings"); received += read; }
    char ssid[33] = {}, password[65] = {}; form[received] = '\0';
    if (!parse_credentials(form, ssid, sizeof(ssid), password, sizeof(password)) || !ssid[0]) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    if (!wifi_save_credentials(ssid, password)) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save Wi-Fi settings");
    return httpd_resp_send(request, "<!doctype html><title>Wi-Fi saved</title><nav><a href=\"/\">Home</a> | <a href=\"/settings\">Settings</a> | <a href=\"/wifi\">Set Wi-Fi</a> | <span id=\"sun-indicator\">Sun events loading...</span></nav><h1>Wi-Fi settings saved</h1><p>Restart the device to connect using the new settings.</p><p><a href=\"/\">Back</a></p><script>fetch('/sun-status').then(r=>r.json()).then(s=>{const clock=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});let text='Mode: '+s.mode+' Time: '+clock+' State: '+(s.mosfet1_on?'ON':'OFF');if(s.mode==='TIMER'){if(s.available){const h=Math.floor(s.minutes/60),d=h?h+'h '+s.minutes%60+'min':s.minutes+'min',at=new Date(s.next_event_unix*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});text+='; '+s.next_event+' in '+d+' @ '+at;}else text+='; '+s.next_event;}document.getElementById('sun-indicator').textContent=text;});</script>", HTTPD_RESP_USE_STRLEN);
}
}

void web_server_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 14;
    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    httpd_uri_t root_uri = {}; root_uri.uri = "/"; root_uri.method = HTTP_GET; root_uri.handler = root; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
    httpd_uri_t settings_uri = {}; settings_uri.uri = "/settings"; settings_uri.method = HTTP_GET; settings_uri.handler = settings_get; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &settings_uri));
    httpd_uri_t location_get_uri = {}; location_get_uri.uri = "/location"; location_get_uri.method = HTTP_GET; location_get_uri.handler = location_get; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &location_get_uri));
    httpd_uri_t location_post_uri = {}; location_post_uri.uri = "/location"; location_post_uri.method = HTTP_POST; location_post_uri.handler = location_post; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &location_post_uri));
    httpd_uri_t sun_get_uri = {}; sun_get_uri.uri = "/sun-settings"; sun_get_uri.method = HTTP_GET; sun_get_uri.handler = sun_settings_get; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &sun_get_uri));
    httpd_uri_t sun_post_uri = {}; sun_post_uri.uri = "/sun-settings"; sun_post_uri.method = HTTP_POST; sun_post_uri.handler = sun_settings_post; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &sun_post_uri));
    httpd_uri_t sun_status_uri = {}; sun_status_uri.uri = "/sun-status"; sun_status_uri.method = HTTP_GET; sun_status_uri.handler = sun_status_get; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &sun_status_uri));
    httpd_uri_t mode_uri = {}; mode_uri.uri = "/mode"; mode_uri.method = HTTP_POST; mode_uri.handler = mode_post; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &mode_uri));
    httpd_uri_t icon_uri = {}; icon_uri.uri = "/favicon.ico"; icon_uri.method = HTTP_GET; icon_uri.handler = favicon; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &icon_uri));
    httpd_uri_t get_uri = {}; get_uri.uri = "/wifi"; get_uri.method = HTTP_GET; get_uri.handler = wifi_get; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));
    httpd_uri_t post_uri = {}; post_uri.uri = "/wifi"; post_uri.method = HTTP_POST; post_uri.handler = wifi_post; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &post_uri));
}
