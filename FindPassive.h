#ifndef FindPassive_h
#define FindPassive_h

#include <vector>

typedef struct wifiSignal {
  char *mac;
  int rssi;
};

class FindPassive {
  public:
    FindPassive(const char* server, const char *group, unsigned long timestamp);
    void AddWifiSignal(char *mac, int rssi);
    String getJSON();
    ~FindPassive();
  private:
    unsigned short _sVersion;
    bool _ishttps;
    unsigned long _timestamp;
    const char *_server;
    const char *_group;
    std::vector<wifiSignal> _wifiSignals;
};
#endif
