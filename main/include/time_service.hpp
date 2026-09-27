#pragma once

#include <cstddef>

void time_service_start();
const char *time_service_format_utc(char *buffer, size_t buffer_size);
