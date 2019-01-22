# esp8266-find-passive

## About
This is an attempt at a [find-lf](https://github.com/schollz/find-lf) source using a single esp8266.
It is based on code from [esp8266mini-sniff](https://github.com/rw950431/ESP8266mini-sniff).

## Usage
This doesn't do anything other than sniff for a few seconds, then connect to wifi and display the generated json over the serial line.
I am posting it to assist anyone else working down the same path.

## Todo
1. Post the client list to a specified find server/group
2. Clean up code
    * Too many global variables
3. Use [WifiManager](https://github.com/tzapu/WiFiManager) for initial setup.
4. Speed up wifi connection
    * Use static ip for reconnecting
    * Maybe use dhcp for initial connect and save ip address to reconnect
    * If using static ip will need to figure out some way to fail if the ip is lost
5. Smarter algorithm for sniffing, I disabled the sniff a channel till no more data in favor of a hardcoded timer.
6. OTA update support?
    * Need to stay under 468KB if I want OTA support
    * Possibly allow ota only on initial boot since this device won't be connected to wifi much and downtime isn't ideal during regular use.
7. HTTPS support
