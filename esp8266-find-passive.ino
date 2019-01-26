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
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          find_server = json["find_server"].asString();
          find_group = json["find_group"].asString();
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
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
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["find_server"] = find_server;
    json["find_group"] = find_group;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(WiFi.localIP());
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
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());
      Serial.print("Heap framentation: ");
      Serial.println(ESP.getHeapFragmentation());
    } else {
      Serial.println(F("Haven't found enough clients to send, continuing monitor mode."));
    }

    previousMillis = millis();
  } else {
    unsigned long newchannel = ((currentMillis - previousMillis) / intmult) + 1;
    if (newchannel != channel) {
      channel = newchannel;
      wifi_set_channel(channel);
    }
    delay(1);
  }
}

#ifdef WIFIMANAGER
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
#endif

void enableWifiMonitorMode()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println(F("Turning on wifi monitoring."));
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(enable);
}

void enableWifiClient()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println(F("Turning off wifi monitoring."));
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
    Serial.print(".");
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(WiFi.localIP());
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
