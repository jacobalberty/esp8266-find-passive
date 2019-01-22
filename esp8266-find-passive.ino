#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>    //https://github.com/arduino-libraries/NTPClient

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#include "functions.h"

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
// note, ESP8266HTTPClient does not support https so the public servers do not work at this time.
String server = "lf.internalpositioning.com"; // find-lf
//String server = "cloud.internalpositioning.com";
const char* group = "FIND_GROUP";

String baseURL = "http://" + server + "/";

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

WiFiClient client;
HTTPClient http;

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

  bool find3;
  String ts;
  http.begin(client, baseURL + "now");
  int res = http.GET();
  Serial.println("Response: " + String(res));
  find3 = (res == 200);
  String timeResponse = http.getString();
  http.end();

  if (find3) {
    ts = timeResponse;
  }
  else {
    if (!timeClient.update()) {
      Serial.println("No NTP response so not sending data.");
      return;
    }
    ts = String(timeClient.getEpochTime());
  }

  Serial.println(getJSON(find3, ts));
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

String getJSON(bool find3, String timestamp) {
  String request;

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  if (find3) {
    root["d"] = String(ESP.getChipId());;
    root["f"] = group;
    root["t"] = timestamp;
    JsonObject& signals = root.createNestedObject("s");
    JsonObject& wifi = signals.createNestedObject("wifi");

    for (int u = 0; u < clients_known_count; u++) {
      wifi[mac2String(clients_known[u].station)] =  clients_known[u].rssi;
    }
  } else {
    root["node"] = String(ESP.getChipId());;
    root["group"] = group;
    root["timestamp"] = timestamp;
    JsonArray & signals = root.createNestedArray("signals");

    for (int u = 0; u < clients_known_count; u++) {
      JsonObject& signal = signals.createNestedObject();
      signal["mac"] = mac2String(clients_known[u].station);
      signal["rssi"] = clients_known[u].rssi;
    }
  }
  root.printTo(request);
  return request;
}

String mac2String(byte mac[]) {
  char macAddr[18];
  sprintf(macAddr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macAddr);
}
