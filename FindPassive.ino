#include "FindPassive.h"
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <NTPClient.h>            //https://github.com/arduino-libraries/NTPClient
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <vector>

FindPassive::FindPassive(String server, String group, short sVersion) {
  _group = group;
  _server = server;
  if (sVersion == -1) {
    HTTPRes http = getHttp("/now"); // Try a URL that only exists in find3
    _sVersion = http.rCode == 200 ? 3 : 2;
  } else {
    _sVersion = sVersion;
  }
}
FindPassive::~FindPassive() {
}

void FindPassive::AddWifiSignal(String mac, int rssi) {
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
        root["timestamp"] = getTimestamp();
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
        root["t"] = getTimestamp();
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

HTTPRes FindPassive::getHttp(String url = "/") {
  HTTPRes res;
  WiFiClient client;
  HTTPClient http;
  const char* headerNames[] = { "Location" };

  size_t found = _server.indexOf("://");

  if (found == -1) {
    _server = "http://" + _server;
  } else if (_server.substring(0, found) == "https") {
    _ishttps = true;
    Serial.println(F("https is not supported at this time"));
#ifdef __EXCEPTIONS
    throw 443;
#endif
  }
  res.url = _server + url;
  http.begin(client, res.url);
  http.collectHeaders(headerNames, sizeof(headerNames) / sizeof(headerNames[0]));

  res.rCode = http.GET();
  if (http.hasHeader("Location"))  {
    String newServer = http.header("Location");
    _server = newServer.substring(0, newServer.lastIndexOf(url));
    http.end();
    return getHttp(url);
  }

  res.body = http.getString();
  http.end();
  return res;
}

unsigned long FindPassive::getTimestamp() {
  switch (_sVersion) {
    case 3: {
        HTTPRes http = getHttp("/now");
        String tst = http.body.substring(0, http.body.length() - 4);
        return tst.toInt();
        break;
      }
    case 2:
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
        return timeClient.getEpochTime();
        break;
      }
  }
}
