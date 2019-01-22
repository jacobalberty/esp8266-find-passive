#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiUdp.h>
#include <NTPClient.h>    //https://github.com/arduino-libraries/NTPClient

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#include "functions.h"

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* group = "FIND_GROUP";

#define MAX_APS_TRACKED 50
#define MAX_CLIENTS_TRACKED 100
#define MIN_CLIENTS_SEND 1

#define disable 0
#define enable  1

bool first = true;
const long interval = 10000;
int nothing_new = 0;

beaconinfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
probeinfo probes_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int probes_known_count = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

unsigned int channel = 1;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

unsigned long previousMillis = 0;
unsigned long intmult = interval / 14;
unsigned long currentMillis;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

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
    } else {
      Serial.println("Haven't found enough clients to send, continuing monitor mode.");
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
  Serial.println("Turning on wifi monitoring.");
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(enable);
}

void enableWifiClient()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println("Turning off wifi monitoring.");
  wifi_promiscuous_enable(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if (!timeClient.update()) {
    Serial.println("No NTP response so not sending data.");
    return;
  }

  String request;

  DynamicJsonBuffer jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
  root["node"] = String(ESP.getChipId());;
  root["group"] = group;
  root["timestamp"] = timeClient.getEpochTime();
  JsonArray& signals = root.createNestedArray("signals");

  for (int u = 0; u < clients_known_count; u++) {
    JsonObject& signal = signals.createNestedObject();
    char macAddr[18];
    uint8_t* mac = clients_known[u].station;
    sprintf(macAddr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    signal["mac"] = macAddr;
    signal["rssi"] = clients_known[u].rssi;
  }
  root.printTo(request);
  Serial.println(request);
  memset(aps_known, 0, MAX_APS_TRACKED);
  aps_known_count = 0;
  memset(clients_known, 0, MAX_CLIENTS_TRACKED);
  clients_known_count = 0;
  memset(probes_known, 0, MAX_CLIENTS_TRACKED);
  probes_known_count = 0;
}
