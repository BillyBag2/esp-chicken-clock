#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_wifi_types.h"

enum class connection_state_t { kConnecting, kConnected, kAccessPoint };

void wifi_initialize();
void wifi_start();
void wifi_update();
connection_state_t wifi_connection_state();
const char *wifi_connection_state_name();
bool wifi_scan(wifi_ap_record_t *networks, uint16_t *network_count);
bool wifi_save_credentials(const char *ssid, const char *password);
