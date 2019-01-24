#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include "FindPassive.h"
#include "functions.h"

// Un-comment this for an experimental quick connect
//#define WIFI_QUICK

// note, ESP8266HTTPClient does not support https so the public servers do not work at this time.
String server = "lf.internalpositioning.com"; // find-lf
//const char *server = "cloud.internalpositioning.com"; // find3
String group = "FIND_GROUP";

#define MAX_APS_TRACKED 10
#define MAX_CLIENTS_TRACKED 50
#define MIN_CLIENTS_SEND 1

#define disable 0
#define enable  1

const long interval = 10000;
int nothing_new = 0;

beaconinfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
probeinfo probes_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int probes_known_count = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

unsigned int channel = 1;

unsigned long previousMillis = 0;
unsigned long intmult = interval / 14;
unsigned long currentMillis;

#ifdef WIFI_QUICK
// Quick reconnect code
struct {
  uint8_t channel;
  uint8_t ap_mac[6];
  String ssid;
  String psk;
} wifiData;
#endif

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFiManager wifiManager;

  wifiManager.autoConnect();

#ifdef WIFI_QUICK
  wifiData.ssid = WiFi.SSID();
  wifiData.psk = WiFi.psk();
  wifiData.channel = WiFi.channel();
  memcpy(wifiData.ap_mac, WiFi.BSSID(), 6 );
#endif

  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(disable);
  wifi_set_promiscuous_rx_cb(promisc_cb);   // Set up promiscuous callback
  enableWifiMonitorMode();
}

void loop() {
  currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    if (clients_known_count >= MIN_CLIENTS_SEND) {
      wifi_promiscuous_enable(disable);
      enableWifiClient();
      enableWifiMonitorMode();
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());
    } else {
      Serial.println(F("Haven't found enough clients to send, continuing monitor mode."));
    }

    previousMillis = currentMillis;
  } else {
    unsigned long newchannel = ((currentMillis - previousMillis) / intmult) + 1;
    if (newchannel != channel) {
      channel = newchannel;
      wifi_set_channel(channel);
    }
    delay(1);
  }
}

void enableWifiMonitorMode()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println(F("Turning on wifi monitoring."));
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(enable);
}

void enableWifiClient()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println(F("Turning off wifi monitoring."));
  wifi_promiscuous_enable(false);
  WiFi.mode(WIFI_STA);
#ifdef WIFI_QUICK
  WiFi.persistent(false);
  WiFi.begin(wifiData.ssid.c_str(), wifiData.psk.c_str(), wifiData.channel, wifiData.ap_mac, true);
  WiFi.persistent(true);
#else
  WiFi.begin();
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(WiFi.localIP());

  FindPassive findPassive = FindPassive(server, group);
  for (int u = 0; u < clients_known_count; u++) {
    findPassive.AddWifiSignal(mac2String(clients_known[u].station), clients_known[u].rssi);
  }
  Serial.println(findPassive.getJSON());
  clearSniffData();
}

void clearSniffData() {
  memset(aps_known, 0, MAX_APS_TRACKED);
  aps_known_count = 0;
  memset(clients_known, 0, MAX_CLIENTS_TRACKED);
  clients_known_count = 0;
  memset(probes_known, 0, MAX_CLIENTS_TRACKED);
  probes_known_count = 0;
}

String mac2String(uint8_t mac[]) {
  static char macAddr[18];

  if (mac == NULL) return "";
  
  snprintf(macAddr, sizeof(macAddr), "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
          
  return String(macAddr);
}
