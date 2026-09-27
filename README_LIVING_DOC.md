# README Living Doc

This file captures questions, ambiguities, inconsistencies, and suggested improvements found while reviewing `README.md`.

## Current repo state

- The repository currently appears to be an early ESP-IDF skeleton project.
- `main/main.c` contains an empty `app_main()` with no implemented timer, Wi-Fi, time sync, GPIO control, web UI, storage, or low-power behavior yet.
- The project name should be kept aligned as `esp-chicken-clock` across the repository, README, and build files.

## Questions to answer in the README

- What exactly is being switched on and off?
- Is the ESP board powering a camera directly, driving a relay, controlling a MOSFET, or sending a control signal to another device?
- What voltage/current limits are expected for the load?
- Is the hardware already wired and tested, or is the README meant to describe a future design?
- What does `ESP32_MOS_X2_V1.1` refer to exactly? A product link or short hardware description would help.
- Is `ESP32-32E N4` the module on the board, an alternative platform, or a required chip variant?
- Should the device turn on before both sunrise and sunset every day, or is the intended behavior:
  - power on before sunrise and off after sunrise, and
  - power on before sunset and off after sunset?
- Why is sunset included for observing chickens waking up? Is the actual use case observing both coop opening and settling for the night?
- What happens if the configured "on" windows overlap, or if the current time is already inside a window at boot?
- What timezone source will be used for sunrise/sunset calculations?
- Will daylight saving time be handled automatically?
- What should happen if time sync fails?
- What should happen if internet is unavailable for multiple days?
- Is location specified by latitude/longitude, address lookup, or browser geolocation?
- Is there a security expectation for the AP mode and web configuration page?
- Should the project depend on an external sunrise/sunset API long term, or should sunrise/sunset eventually be calculated locally from latitude, longitude, and date?

## Ambiguities and wording issues

- "An esp based timer for a camera for observing when chickens go to sleep or wake up." is understandable, but could be clearer about the system boundary and user value.
- "Uses off the shelf board ESP32_MOS_X2_V1.1." assumes the reader already knows the board.
- "Will switch on power a configurable number of minutes before sun rise and sun set and turn off a configurable number of minutes after rise and set." is the core behavior, but it would benefit from a precise example timeline.
- The new sunrise/sunset section proposes using an internet API, but it does not yet define refresh frequency, caching rules, timezone handling, or fallback behavior in enough detail to guide implementation.
- "sun rise" / "sun set" / "wifi" capitalization and spelling are inconsistent. Standardizing to `sunrise`, `sunset`, and `Wi-Fi` would make the document feel more finished.

## Missing documentation

- No setup instructions for the development environment.
- No ESP-IDF version requirement.
- No build instructions.
- No flash instructions.
- No serial monitor instructions.
- No hardware wiring notes.
- No GPIO pin assignments.
- No configuration storage approach.
- No detailed explanation of how sunrise/sunset API responses will be validated, cached, or converted to local time.
- No explanation of how the device behaves before it has valid time.
- No explanation of failure modes or fallback behavior.
- No acceptance criteria for the TODO items.

## Suggested README improvements

- Add a short "Status" section near the top that clearly says this project is in planning / scaffold stage.
- Add a "Goal" section describing the intended user-facing behavior in one paragraph.
- Add an "Expected daily behavior" section with a concrete example:
  - Example: sunrise 06:20, sunset 18:05, turn on 20 minutes before, turn off 10 minutes after.
- Add a "Hardware" section listing the exact board, module, output method, and connected load.
- Add a "Software architecture" section covering:
  - time source,
  - sunrise/sunset source or algorithm,
  - Wi-Fi station/AP fallback flow,
  - settings persistence,
  - power control logic,
  - sleep/wake behavior.
- Add a "Current implementation status" section so readers can see what is planned versus already built.
- Convert the TODO list into grouped milestones such as `Networking`, `Time`, `Sun events`, `Web UI`, `Persistence`, and `Power management`.
- Replace vague items with testable statements. Example:
  - Instead of "Try to log into the existing WiFi."
  - Use "Attempt station-mode connection using saved SSID/password for up to 60 seconds."
- Add a section explicitly documenting assumptions and open decisions.
- Add a short note saying the current README proposes `sunrise-sunset.org` as the initial source, but this is still a design choice rather than implemented behavior.

## Inconsistencies to resolve

- README describes substantial planned behavior, but the codebase currently contains only a blank application entry point.
- The README now proposes an external sunrise/sunset API, but the repo does not yet contain networking, JSON parsing, time sync, or persistence code to support it.

## Recommended README structure

- Project summary
- Status
- Intended behavior
- Hardware
- Software design
- Setup and build
- Flash and monitor
- Configuration model
- Open questions
- Roadmap

## Potential technical decisions to document later

- Whether sunrise/sunset is obtained from an internet API or calculated locally from lat/long and date.
- Whether NTP alone is sufficient, or whether RTC hardware is needed for resilience.
- Whether the AP configuration portal should always be available via button press as a recovery path.
- How credentials and settings are stored securely in flash/NVS.
- What safe default output state should be used on boot and on failure.

## Suggested next pass

- Rewrite `README.md` so it clearly separates:
  - what the project is,
  - what is already implemented,
  - what decisions are still open,
  - what work is planned next.
