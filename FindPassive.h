#ifndef FindPassive_h
#define FindPassive_h

#include <vector>

typedef struct HTTPRes {
  String url;
  String body;
  int rCode;
};

typedef struct wifiSignal {
  String mac;
  int rssi;
};

class FindPassive {
  public:
    FindPassive(String server, String group);
    void AddWifiSignal(String mac, int rssi);
    String getJSON();
    ~FindPassive();
  private:
    HTTPRes getHttp(String url);
    short _sVersion = -1;
    bool _ishttps;
    unsigned long _timestamp;
    String _server;
    String _group;
    std::vector<wifiSignal> _wifiSignals;
};
#endif
