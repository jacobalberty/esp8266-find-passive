#include "FindPassive.h"

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <vector>
FindPassive::FindPassive(const char *server, const char *group, unsigned long timestamp) {
  _sVersion = 2;
  _server = server;
  _group = group;
  _timestamp = timestamp;
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
