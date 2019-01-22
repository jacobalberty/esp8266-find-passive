#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiUdp.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#include "functions.h"

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* group = "FIND_GROUP";

#define MAX_APS_TRACKED 50
#define MAX_CLIENTS_TRACKED 100
#define MIN_CLIENTS_SEND 1

#define disable 0
#define enable  1

bool first = true;
const long interval = 10000;
int nothing_new = 0;

beaconinfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
probeinfo probes_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int probes_known_count = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs

unsigned int channel = 1;

WiFiUDP UDP;                     // Create an instance of the WiFiUDP class to send and receive

IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message

byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;
unsigned long previousMillis = 0;
unsigned long intmult = interval / 14;
unsigned long currentMillis;
unsigned long prevActualTime = 0;
uint32_t actualTime = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  if (!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
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
    } else {
      Serial.println("Haven't found enough clients to send, continuing monitor mode.");
    }

    previousMillis = currentMillis;
  } else {
    unsigned long newchannel = ((currentMillis - previousMillis) / intmult) + 1;
    if (newchannel != channel) {
      channel = newchannel;
      wifi_set_channel(channel);
    }
    delay(1);
  }
}

void enableWifiMonitorMode()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println("Turning on wifi monitoring.");
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(enable);
}

void enableWifiClient()
{
  // Send ESP into promiscuous mode. At this point, it stops being able to connect to the internet
  Serial.println("Turning off wifi monitoring.");
  wifi_promiscuous_enable(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  UDP.begin(123);

  sendNTPpacket(timeServerIP);               // Send an NTP request
  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
  } else if ((currentMillis - lastNTPResponse) > 3600000) {
    Serial.println("More than 1 hour since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  } else {
    Serial.println("No NTP response yet so not sending");
    return;
  }
  actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
  if (actualTime != prevActualTime && timeUNIX != 0) { // If a second has passed since last print
    prevActualTime = actualTime;
  }

  String request;

  DynamicJsonBuffer jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
  root["node"] = String(ESP.getChipId());;
  root["group"] = group;
  root["timestamp"] = actualTime;
  JsonArray& signals = root.createNestedArray("signals");

  for (int u = 0; u < clients_known_count; u++) {
    JsonObject& signal = signals.createNestedObject();
    char macAddr[18];
    uint8_t* mac = clients_known[u].station;
    sprintf(macAddr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    signal["mac"] = macAddr;
    signal["rssi"] = clients_known[u].rssi;
  }
  root.printTo(request);
  Serial.println(request);
  memset(aps_known, 0, MAX_APS_TRACKED);
  aps_known_count = 0;
  memset(clients_known, 0, MAX_CLIENTS_TRACKED);
  clients_known_count = 0;
  memset(probes_known, 0, MAX_CLIENTS_TRACKED);
  probes_known_count = 0;
}

uint32_t getTime() {
  unsigned long curMillis = millis();
  unsigned long startMillis = curMillis;
  int packetSize = UDP.parsePacket();
  while (packetSize < 1) {
    curMillis = millis();
    if (curMillis - startMillis >= 250) {
      return 0; // We timed out so just return 0
    }
    packetSize = UDP.parsePacket();
    yield();
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  NTPBuffer[1] = 0;     // Stratum, or type of clock
  NTPBuffer[2] = 6;     // Polling Interval
  NTPBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  NTPBuffer[12]  = 49;
  NTPBuffer[13]  = 0x4E;
  NTPBuffer[14]  = 49;
  NTPBuffer[15]  = 52;

  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}
