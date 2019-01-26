#include "options.h"
#ifdef WIFIMANAGER
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#endif
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#ifdef WIFIMANAGER
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#include <WiFiManager.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#endif

#ifdef DEBUG
#define DPRINT(x) Serial.print(x)
#define DPRINTLN(x) Serial.println(x)
#else
#define DPRINT(x)
#define DPRINTLN(x)
#endif

#include "FindPassive.h"
#include "functions.h"

String find_server = FIND_SERVER;
String find_group = FIND_GROUP;

#define disable 0
#define enable  1

const long interval = SNIFF_INTERVAL;
int nothing_new = 0;

beaconinfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
probeinfo probes_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int probes_known_count = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

unsigned int channel = 1;

unsigned long previousMillis = 0;
unsigned long intmult = interval / 14;
unsigned long currentMillis;

#ifdef WIFI_QUICK
// Quick reconnect code
struct {
  uint8_t channel;
  uint8_t ap_mac[6];
  String ssid;
  String psk;
} wifiData;
#endif


#ifdef WIFIMANAGER
//flag for saving data
bool shouldSaveConfig = false;
#endif

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
#ifdef WIFIMANAGER
  //read configuration from FS json
  DPRINTLN("mounting FS...");

  if (SPIFFS.begin()) {
    DPRINTLN("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      DPRINTLN("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DPRINTLN("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          DPRINTLN("\nparsed json");

          find_server = json["find_server"].asString();
          find_group = json["find_group"].asString();
        } else {
          DPRINTLN("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    DPRINTLN("failed to mount FS");
  }

  WiFiManagerParameter custom_find_server("server", "find server", find_server.c_str(), 40);
  WiFiManagerParameter custom_find_group("group", "find group", find_group.c_str(), 40);

  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_find_server);
  wifiManager.addParameter(&custom_find_group);

  wifiManager.autoConnect();

  find_server = custom_find_server.getValue();
  find_group = custom_find_group.getValue();

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DPRINTLN("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["find_server"] = find_server;
    json["find_group"] = find_group;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DPRINTLN("failed to open config file for writing");
    }
#ifdef DEBUG
    json.printTo(Serial);
#endif
    json.printTo(configFile);
    configFile.close();
    //end save
  }
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DPRINT(".");
  }
  DPRINTLN("");
  DPRINTLN("WiFi connected");
  DPRINTLN(WiFi.localIP());
#endif

#ifdef WIFI_QUICK
  wifiData.ssid = WiFi.SSID();
  wifiData.psk = WiFi.psk();
  wifiData.channel = WiFi.channel();
  memcpy(wifiData.ap_mac, WiFi.BSSID(), 6 );
#endif

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
      DPRINT("Free heap: ");
      DPRINTLN(ESP.getFreeHeap());
      DPRINT("Heap framentation: ");
      DPRINTLN(ESP.getHeapFragmentation());
    } else {
      DPRINTLN("Haven't found enough clients to send, continuing monitor mode.");
    }

    previousMillis = millis();
  } else {
    channel = 1;
    while (true) {
      nothing_new++;                          // Array is not finite, check bounds and adjust if required
      if (nothing_new > 200) {
        nothing_new = 0;
        channel++;
        if (channel == 15) break;             // Only scan channels 1 to 14
        wifi_set_channel(channel);
      }
      delay(1);  // critical processing timeslice for NONOS SDK! No delay(0) yield()
    }
  }
}

#ifdef WIFIMANAGER
//callback notifying us of the need to save config
void saveConfigCallback () {
  DPRINTLN("Should save config");
  shouldSaveConfig = true;
}
#endif

void enableWifiMonitorMode()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  DPRINTLN("Turning on wifi monitoring.");
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(enable);
}

void enableWifiClient()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  DPRINTLN("Turning off wifi monitoring.");
  wifi_promiscuous_enable(false);
  WiFi.mode(WIFI_STA);
#ifdef WIFI_QUICK
  WiFi.persistent(false);
  WiFi.begin(wifiData.ssid.c_str(), wifiData.psk.c_str(), wifiData.channel, wifiData.ap_mac, true);
  WiFi.persistent(true);
#else
  WiFi.begin();
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DPRINT(".");
  }
  DPRINTLN("");
  DPRINTLN("WiFi connected");
  DPRINTLN(WiFi.localIP());
#ifdef FIND_VERSION
  FindPassive findPassive = FindPassive(find_server, find_group, FIND_VERSION);
#else
  FindPassive findPassive = FindPassive(find_server, find_group);
#endif
  for (int u = 0; u < clients_known_count; u++) {
    findPassive.AddWifiSignal(mac2String(clients_known[u].station), clients_known[u].rssi);
  }
  HTTPRes hres = findPassive.sendData();
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

String mac2String(uint8_t mac[]) {
  static char macAddr[18];

  if (mac == NULL) return "";

  snprintf(macAddr, sizeof(macAddr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(macAddr);
}
