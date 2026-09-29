#pragma once

struct location_t {
    double latitude;
    double longitude;
};

bool location_load(location_t *location);
bool location_save(const location_t &location);
