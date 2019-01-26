#ifndef Options_h
#define Options_h

// Enable WiFiManager configuration
#define WIFIMANAGER // https://github.com/tzapu/WiFiManager
// Un-comment this for an experimental quick connect
//#define WIFI_QUICK
// Un-comment this to include timestamp with submission
//#define INCLUDE_TIMESTAMP
// Un-comment to include debug information
//#define DEBUG

// Used if WiFiManager disabled
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PSK "WIFI_PSK"

// note, ESP8266HTTPClient does not support https so the public servers do not work at this time.
//#define FIND_SERVER "lf.internalpositioning.com" // find-lf
#define FIND_SERVER "cloud.internalpositioning.com" // find3
#define FIND_GROUP "FIND_GROUP"
//#define FIND_VERSION 3 // Use to turn off version autodetection and manually specificy server version
#define FIND_NODE String(ESP.getChipId()) // Needs to be unique to the device and stay the same.

#define MAX_APS_TRACKED 25
#define MAX_CLIENTS_TRACKED 75
#define MIN_CLIENTS_SEND 1
#define SNIFF_INTERVAL 10000

#endif
