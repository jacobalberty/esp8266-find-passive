#ifndef FindPassive_h
#define FindPassive_h

#include <vector>

typedef struct HTTPUrl {
  String proto;
  String host;
  String path;
  uint16_t port;
};

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
    FindPassive(String server, String group, short sVersion = -1);
    void AddWifiSignal(String mac, int rssi);
    String getJSON();
    HTTPRes sendData();
    ~FindPassive();
  private:
    HTTPRes getHttp(String url);
    HTTPUrl parseURL(String url);
#ifdef INCLUDE_TIMESTAMP
    unsigned long getTimestamp();
#endif
    short _sVersion = -1;
    bool _ishttps;
    String _server;
    String _group;
    std::vector<wifiSignal> _wifiSignals;
};
#endif
