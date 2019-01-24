#include "FindPassive.h"
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <NTPClient.h>            //https://github.com/arduino-libraries/NTPClient
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <vector>

FindPassive::FindPassive(String server, String group) {
  init(server, group);
}
void FindPassive::init(String server, String group) {
  WiFiClient client;
  HTTPClient http;
  _server = server;
  _group = group;

  size_t found = server.indexOf("://");

  if (found == -1) {
    server = "http://" + server;
  } else if (server.substring(0, found) == "https") {
    _ishttps = true;
    Serial.println(server);
    Serial.println(F("https is not supported at this time"));
    throw 443;
  }
  const char* headerNames[] = { "Location" };

  http.begin(client, server + "/now");
  http.collectHeaders(headerNames, sizeof(headerNames) / sizeof(headerNames[0]));

  int res = http.GET();
  if (http.hasHeader("Location"))  {
    String newServer = http.header("Location");
    init(newServer.substring(0, newServer.lastIndexOf("/now")), group);
    return;
  }
  
  switch (res) {
    case 200: {
        _sVersion = 3;
        _timestamp = atoi(http.getString().c_str());
        break;
      }
    default: {
        WiFiUDP ntpUDP;
        NTPClient timeClient(ntpUDP);

        _sVersion = 2;
        timeClient.begin();
        unsigned long startMillis = millis();
        unsigned long currentMillis = startMillis;
        while (!timeClient.update()) {
          currentMillis = millis();
          if (currentMillis - startMillis >= 1000) {
            break;
          }
        }
        _timestamp = timeClient.getEpochTime();
      }
  }
  http.end();
}

FindPassive::~FindPassive() {
}
void FindPassive::AddWifiSignal(char *mac, int rssi) {
  _wifiSignals.push_back({mac, rssi});
}

String FindPassive::getJSON() {
  String request;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  switch (_sVersion) {
    case 2: {
        root["node"] = String(ESP.getChipId());
        root["group"] = group;
        root["timestamp"] = _timestamp;
        JsonArray& signals = root.createNestedArray("signals");
        for (wifiSignal s : _wifiSignals) {
          JsonObject& signal = signals.createNestedObject();
          signal["mac"] = s.mac;
          signal["rssi"] = s.rssi;
        }
        break;
      }
    case 3: {
        root["d"] = String(ESP.getChipId());
        root["f"] = group;
        root["t"] = _timestamp;
        JsonObject& signals = root.createNestedObject("s");
        JsonObject& wifi = signals.createNestedObject("wifi");
        for (wifiSignal s : _wifiSignals) {
          wifi[s.mac] = s.rssi;
        }
        break;
      }
  }
  root.printTo(request);
  return request;

}
