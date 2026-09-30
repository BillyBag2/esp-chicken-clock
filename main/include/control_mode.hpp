#pragma once

enum class control_mode_t { kTimer, kOn, kOff };

bool control_mode_load(control_mode_t *mode);
bool control_mode_save(control_mode_t mode);
const char *control_mode_name(control_mode_t mode);
