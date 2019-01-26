#include "FindPassive.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#ifdef INCLUDE_TIMESTAMP
#include <NTPClient.h>            //https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>
#endif
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
        root["node"] = FIND_NODE;
        root["group"] = _group;
#ifdef INCLUDE_TIMESTAMP
        root["timestamp"] = getTimestamp();
#endif
        JsonArray& signals = root.createNestedArray("signals");
        for (wifiSignal s : _wifiSignals) {
          JsonObject& signal = signals.createNestedObject();
          signal["mac"] = s.mac;
          signal["rssi"] = s.rssi;
        }
        break;
      }
    case 3: {
        root["d"] = FIND_NODE;
        root["f"] = _group;
#ifdef INCLUDE_TIMESTAMP
        root["t"] = getTimestamp();
#endif
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

HTTPRes FindPassive::sendData() {
  HTTPUrl purl = parseURL(_server + "/passive");
  HTTPRes res;
  HTTPClient http;
  WiFiClient client;

  res.url = purl.proto + "://" + purl.host + ":" + String(purl.port) + purl.path;

  http.begin(client, purl.host, purl.port, purl.path);
  http.addHeader("Content-Type", "application/json");
  res.rCode = http.POST(getJSON());
  http.end();

  return res;
}

HTTPRes FindPassive::getHttp(String url = "/") {
  HTTPRes res;
  HTTPUrl purl = parseURL(_server + url);
  WiFiClient client;
  HTTPClient http;
  const char* headerNames[] = { "Location" };


  if (purl.proto == "https") {
    _ishttps = true;
    Serial.println(F("https is not supported at this time"));
#ifdef __EXCEPTIONS
    throw 443;
#endif
  }
  res.url = purl.proto + "://" + purl.host + ":" + String(purl.port) + purl.path;
  http.begin(client, purl.host, purl.port, purl.path);
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

HTTPUrl FindPassive::parseURL(String url) {
  String tmp;
  HTTPUrl res;
  size_t found;

  // Determine protocol
  found = url.indexOf("://");
  if (found == -1) {
    res.proto = "http";
    tmp = url;
  } else {
    res.proto = url.substring(0, found);
    tmp = url.substring(found + 3);
  }

  // Determine path
  found = tmp.indexOf("/");
  if (found == -1) {
    res.host = tmp;
  } else {
    res.host = tmp.substring(0, found);
    res.path = tmp.substring(found);
  }

  // Determine port
  found = res.host.indexOf(":");
  if (found == -1) {
    if (res.proto == "https")
      res.port = 443;
    else
      res.port = 80;
  } else {
    res.port = res.host.substring(found + 1).toInt();
    res.host = res.host.substring(0, found);
  }
  return res;
}

#ifdef INCLUDE_TIMESTAMP
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
#endif
