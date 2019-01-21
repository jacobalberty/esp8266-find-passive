# esp8266-find-passive

## About
This is an attempt at a [find-lf](https://github.com/schollz/find-lf) source using a single esp8266.

## Usage
This doesn't do anything other than sniff for a few seconds, then connect to wifi and push how many clients it has seen out over the serial lines.
I am posting it to assist anyone else working down the same path.

## Todo
1. Turn the client list into JSON
2. Post the client list to a specified find server/group
3. Use [WifiManager](https://github.com/tzapu/WiFiManager) for initial setup.
