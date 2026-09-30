#pragma once

struct sun_schedule_status_t {
    const char *mode;
    bool available;
    bool mosfet1_on;
    const char *next_event;
    int minutes_to_next_event;
    long long next_event_unix;
};

void sun_schedule_update();
sun_schedule_status_t sun_schedule_status();
