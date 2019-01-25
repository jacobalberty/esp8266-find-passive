# esp8266-find-passive

## About
This is an attempt at a [find-lf](https://github.com/schollz/find-lf) source using a single esp8266.
It is based on code from [esp8266mini-sniff](https://github.com/rw950431/ESP8266mini-sniff).

## Dependencies
* [WiFiManager](https://github.com/tzapu/WiFiManager)
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
* [NTPClient](https://github.com/arduino-libraries/NTPClient) (Optional only needed if you enable INCLUDE_TIMESTAMP)

## Usage
To use this you will will need to install all of the dependencies.
Currently the public servers DO NOT work as this does not yet support https.
In the arduino IDE select a flash size that includes at least 32k spiffs.
compile and upload using the Arduino IDE. If you do not have any valid wireless settings saved yet
WiFiManager will launch, you simply look for an SSID named "ESP" followed by a bunch of numbers, connect to it and the captive portal will
guide you through connecting, be sure to change the find server and group as the public servers do not currently work since we don't (yet) support https.

## Todo
1. HTTPS support
2. Clean up code
    * Too many global variables
3. Speed up wifi connection
    * Use static ip for reconnecting
    * Maybe use dhcp for initial connect and save ip address to reconnect
    * If using static ip will need to figure out some way to fail if the ip is lost
4. Smarter algorithm for sniffing, I disabled the sniff a channel till no more data in favor of a hardcoded timer.
5. OTA update support?
    * Need to stay under 468KB if I want OTA support
    * Possibly allow ota only on initial boot since this device won't be connected to wifi much and downtime isn't ideal during regular use.

