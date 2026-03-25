# esp-chicken-clock

An esp based solid state MOSFET "switch" for power to a security camera. The camera operates when the chickens are going to sleep and waking up. The battery life is limited and the camera is powered off apart from these two periods each day.

It uses off the shelf board ESP32_MOS_X2_V1.1. That uses a solid state "switch" to control power to the security camera.

The ESP switches on power a configurable number of minutes before sun rise and sun set and turn off a configurable number of minutes after rise and set.

So it could be 20 minutes before sun rise to 30 minutes after sun rise. Then 40 minutes before sun set to 10 minutes after.

The Chickens are let out when they wake up and shut away when they roost at the end of the day. Their feed, water and health are also checked at these times. An automatically controlled door is undesirable for this reason. However the exact time is up to the Chickens, so a camera helps see them when waking or roosting so the checks are done at the correct time.

## Platform

A commonly available board, ESP32_MOS_X2_V1.1 has an ESP32-32E N4 and a two channel MOSFET solid state "switch".

For example see this [ebay listing](https://www.ebay.co.uk/itm/116844380922?mkevt=1&mkcid=1&mkrid=710-53481-19255-0&campid=5339120189&toolid=20006&_trkparms=ispr%3D1&amdata=enc%3A1d78a6yQiQIeqTEbtbzlFGA9&customid=Cj0KCQjwj47OBhCmARIsAF5wUEGuFtYBioE1y2s-A1Lv1kkPV3mQ02b7OoKi_7jXO-JznEeG0ic16DcaAqexEALw_wcB|0AAAAAoWc2zfFqfXsZoUM5xj-FHyoyEnEG|CkAKCAjw7IjOBhBsEjAAfOqUkmNUceylByIwvy4g4DYQlOXvM-fABOtCWBHlzdvg7H338znZRpVVScXHcB8aApfF&gclid=Cj0KCQjwj47OBhCmARIsAF5wUEGuFtYBioE1y2s-A1Lv1kkPV3mQ02b7OoKi_7jXO-JznEeG0ic16DcaAqexEALw_wcB&gbraid=0AAAAAoWc2zfFqfXsZoUM5xj-FHyoyEnEG&wbraid=CkAKCAjw7IjOBhBsEjAAfOqUkmNUceylByIwvy4g4DYQlOXvM-fABOtCWBHlzdvg7H338znZRpVVScXHcB8aApfF&loc_interest_ms=&loc_physical_ms=9193146&adtype=pla&gad_source=1&gad_campaignid=23233019726&gbraid=0AAAAAoWc2zfFqfXsZoUM5xj-FHyoyEnEG).

## Obtaining sun set/sun rise.

Proposed method of obtaining sun rise/set knowing the location.

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
* [ ] Add to README.txt a strategy for obtaining sun set/sun rise from internet.
* [ ] Add to web site the ability to set the location (coordinates) for sun set and sun rise.
* [ ] Store setting even if power is lost.
* [ ] In website have ability to configure the four setting of minutes before/after rise/set.
* [ ] Enter a low power mode when the timer can be off. Sleep for 5minutes, wake and then go to sleep again.
* [ ] When the time is less than the prescribed number of minutes before the rise/set turn on. Turn off the prescribed number of minutes after the rise/set.
