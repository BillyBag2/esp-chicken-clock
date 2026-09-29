#pragma once

struct sun_schedule_status_t {
    bool available;
    bool mosfet1_on;
    const char *next_event;
    int minutes_to_next_event;
};

void sun_schedule_update();
sun_schedule_status_t sun_schedule_status();
