# esp-chicken-clock

## Introduction

Chickens go to roost as the light fades and want to be let out as the sun rises. However they are not like clockwork, so a camera is used to observe them. However to solar charged batter has limited life so a ESP32_MOS_X2_V1.1 board is used to switch power to the camera on and off at the right time.

## Requirements

Switch on the camera a configurable number of minutes before sun set.

## Platform

A commonly available board, ESP32_MOS_X2_V1.1 has an ESP32-32E N4 and a two channel MOSFET solid state "switch".

![ESP32_MOS_X2_V1.1](images/board.jpg)

## GPIO pin assignments

GPIO16, GPIO17, (GPIO26, and GPIO27)

LED: GPIO23

## Obtaining sun set/sun rise

Proposed approach:

* The user configures the device location as latitude and longitude. (From browser?)
* The device obtains the current date and time from NTP when connected to Wi-Fi.
* Sunrise and sunset times are then obtained from an internet service using that location and date.
* A good option is the free `sunrise-sunset.org` API because it accepts latitude and longitude and returns sunrise and sunset times for a given date.
* If internet access is unavailable, the device can keep using the most recently fetched sunrise/sunset values until it is able to refresh them.

## TODO

* [ ] Add web site. This does not need to be secure.
* [ ] Web site includes configuration for an existing WiFi id/password.
* [ ] Try to log into the existing WiFi.
* [ ] If after 60 seconds unable connect to existing wifi, become an AP.
* [ ] After 1 min of no AP activity, try to connect to existing wifi again.
* [ ] AP configuration is fixed. A fixed password is used to access the WiFi securely.
* [ ] The AP provides the "capture" feature to redirect android/iphone and windows devices to the web site.
* [ ] Add button to web site to obtain time/date from browser.
* [ ] Have hard wired list of network time servers. Does not need to be user configurable.
* [ ] When connected to existing WiFi contact network time servers for the time.
* [ ] Add to README.md a strategy for obtaining sun set/sun rise from internet.
* [ ] Add to web site the ability to set the location (coordinates) for sun set and sun rise.
* [ ] Store setting even if power is lost.
* [ ] In website have ability to configure the four setting of minutes before/after rise/set.
* [ ] Enter a low power mode when the timer can be off. Sleep for 5minutes, wake and then go to sleep again.
* [ ] When the time is less than the prescribed number of minutes before the rise/set turn on. Turn off the prescribed number of minutes after the rise/set.
* [ ] The timing is related to sun rise/set only. There is no requirement to show day light saving. However, browser time, network time and the timings of rise and set must be managed for this to be correct. Eliminating day light saving from the logic would be easiest, if possible.