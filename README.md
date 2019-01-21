# esp8266-find-passive

## About
This is an attempt at a [find-lf](https://github.com/schollz/find-lf) source using a single esp8266.
It is based on code from [esp8266mini-sniff](https://github.com/rw950431/ESP8266mini-sniff).

## Usage
This doesn't do anything other than sniff for a few seconds, then connect to wifi and display the generated json over the serial line.
I am posting it to assist anyone else working down the same path.

## Todo
1. Time support for the timestamp.
2. Post the client list to a specified find server/group
3. Use [WifiManager](https://github.com/tzapu/WiFiManager) for initial setup.
4. OTA update support?
