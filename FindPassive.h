#ifndef FindPassive_h
#define FindPassive_h

#include <vector>

typedef struct wifiSignal {
  char *mac;
  int rssi;
};

class FindPassive {
  public:
    FindPassive(String server, String group);
    void AddWifiSignal(char *mac, int rssi);
    String getJSON();
    ~FindPassive();
  private:
    void init(String server, String group);
    unsigned short _sVersion;
    bool _ishttps;
    unsigned long _timestamp;
    String _server;
    String _group;
    std::vector<wifiSignal> _wifiSignals;
};
#endif
