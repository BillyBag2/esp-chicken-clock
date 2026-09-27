# ESP Chicken Clock: Contributor Guide

## Project overview

This is an ESP-IDF firmware project for an ESP32-based chicken-camera power
timer. The intended device will control a camera around sunrise and sunset.
The firmware includes Wi-Fi provisioning, NVS-backed credentials, NTP time,
mDNS, LED status behavior, and a small web interface. Sunrise/sunset scheduling
and camera power-control behavior remain planned work.

The checked-in configuration targets `esp32` (ESP32-32E N4 on the
ESP32_MOS_X2_V1.1 board) and was last built with ESP-IDF v6.0. The serial
monitor baud rate is 115200.

## Repository layout

- `CMakeLists.txt` -- top-level ESP-IDF project definition; project name is
  `esp-chicken-clock`.
- `main/` -- application component. Register new source files and component
  dependencies in `main/CMakeLists.txt`.
- `main/main.cpp` -- firmware startup and main loop.
- `main/wifi.cpp`, `main/time_service.cpp`, `main/led.cpp`, and
  `main/web_server.cpp` -- focused runtime modules; their public interfaces are
  in `main/include/`.
- `sdkconfig` -- current ESP-IDF configuration for the ESP32 target.
- `README.md` -- product intent and roadmap.
- `README_LIVING_DOC.md` -- open documentation questions and proposed README
  improvements; keep it aligned when resolving documented decisions.
- `images/` -- documentation assets.

## ESP-IDF workflow (PowerShell)

Start an ESP-IDF PowerShell environment, or initialise the installed ESP-IDF
before using `idf.py`:

```powershell
& C:\esp\v6.0\esp-idf\export.ps1
```

Run these commands from the repository root:

```powershell
idf.py set-target esp32       # only when changing or recreating target config
idf.py build
idf.py -p COMx flash          # replace COMx with the board's serial port
idf.py -p COMx monitor
idf.py -p COMx flash monitor
```

Use `idf.py fullclean` only when a normal build cannot recover from stale
generated files; it removes the local `build/` directory. Do not commit build
outputs or other generated artifacts.

## Implementation conventions

- Write firmware in C++ and follow the existing ESP-IDF/CMake component layout.
- Add each ESP-IDF component dependency to `REQUIRES` or `PRIV_REQUIRES` in
  `main/CMakeLists.txt`; do not rely on incidental transitive dependencies.
- Use ESP-IDF APIs and error conventions (`esp_err_t`, `ESP_ERROR_CHECK`, and
  component logging) rather than host-platform substitutes.
- Keep hardware-specific GPIO assignments in named constants with comments
  identifying the board/output. There are currently no verified pin mappings;
  do not invent one from the board name or image.
- Initialise controlled outputs to a documented safe state before starting
  networking, time synchronisation, or background tasks.
- Treat Wi-Fi credentials and user settings as sensitive. Never place real
  credentials, tokens, or device-specific settings in source, documentation,
  or committed configuration.
- Prefer UTC internally for time calculations. Make timezone, daylight-saving,
  sunrise/sunset source, API caching, and offline fallback behaviour explicit
  before implementing them.

## Verification and change scope

- For firmware or CMake changes, run `idf.py build` and report the result.
- Flashing, monitoring, erasing flash, and changing persistent device settings
  require a connected physical device; do these only when the task requests it.
- Run targeted ESP-IDF tests when tests are added. There is currently no
  project-specific automated test suite.
- Preserve unrelated working-tree changes. The repository may contain local
  documentation and build changes made by another contributor.
- Keep `README.md` precise about what is implemented versus planned. When a
  design decision is made, update `README_LIVING_DOC.md` to remove or revise
  the corresponding open question.
