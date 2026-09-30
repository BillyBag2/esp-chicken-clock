# README Living Doc

This document records the current implementation and the decisions still to be
made. Keep it aligned with `README.md` as functionality changes.

## Current implementation

- The firmware is an ESP-IDF C++ application for an ESP32-32E N4 on the
  ESP32_MOS_X2_V1.1 board.
- GPIO23 drives the status LED; GPIO16 and GPIO17 are the two MOSFET outputs;
  GPIO0 is the boot button. MOSFET outputs are set low during startup.
- Wi-Fi credentials are stored in NVS. The device tries station mode for ten
  seconds, then starts the password-protected `ChickenClock-Setup` AP if it
  cannot connect or no credentials exist.
- While in AP mode, a client joining/leaving or using the web interface counts
  as activity. After one minute without activity, saved station credentials are
  retried.
- The device hostname and mDNS name are `chicken`, so `chicken.local` is
  advertised on compatible local networks.
- NTP uses a fixed list of public servers. The web interface displays local
  browser time and UTC time, but scheduling uses UTC internally.
- The Settings page stores map-selected latitude/longitude and signed sunset
  and dusk offsets in NVS. It uses Leaflet with OpenStreetMap tiles for the
  map, and displays a seven-day Sunrise-Sunset.org table.
- After Wi-Fi is connected and NTP is valid, the firmware waits ten seconds,
  fetches today and tomorrow from Sunrise-Sunset.org, and keeps both event
  pairs in RAM. The first failed fetch retries after ten seconds; later failed
  fetches retry after five minutes.
- MOSFET 1 is driven high from `sunset + sunset offset` until
  `dusk + dusk offset`. The shared web header shows the FET state and an
  on/off countdown rounded to the nearest minute.
- Home also provides persistent `TIMER`, forced `ON`, and forced `OFF` modes.
  In timer mode the header includes the next transition and its browser-local
  scheduled time.

## Open decisions and gaps

- Confirm with measurement that a high GPIO level is the intended active level
  for MOSFET 1 on the actual board and camera wiring. The board documentation
  does not provide a complete schematic in this repository.
- Document camera voltage, current, polarity, fuse/protection requirements,
  and safe behavior on reset or brownout.
- Decide whether the fixed AP password and unencrypted HTTP configuration page
  are acceptable for the deployment environment.
- Captive-portal behavior for phones and computers is not implemented.
- A sunrise window and the other rise/set offset settings are not implemented.
- Low-power sleep is not implemented.
- The two-day sun-event cache is RAM-only. Define the desired behavior after a
  power loss, a prolonged network outage, or an NTP failure; an RTC and/or
  NVS-backed event cache may be appropriate.
- Define whether scheduling must continue through a Wi-Fi disconnect after
  events have been cached. The current update path only changes the schedule
  while station Wi-Fi is connected.
- Decide whether the external Sunrise-Sunset.org dependency should remain or
  be replaced with local astronomical calculations.

## Documentation work still useful

- Add concise ESP-IDF setup, build, flash, and serial-monitor instructions to
  `README.md`.
- Add a hardware wiring and electrical-limits section, with a verified board
  source or schematic.
- State the AP password change procedure and the security limitations of the
  configuration UI.
- Add a simple operational timeline showing station connection, AP fallback,
  NTP synchronisation, event fetch, and MOSFET control.

## Verification status

- `git diff --check` is used for repository changes.
- A firmware build should be run for each firmware/CMake change. At the last
  attempt, `idf.py build` could not start because the ESP-IDF Python virtual
  environment (`idf6.1_py3.11_env`) is missing from this development machine.
