//parse out details of client frame
struct clientinfo parse_data(uint8_t *frame, uint16_t framelen, signed rssi, unsigned channel)
{
  // takes 36 byte frame control frame
  struct clientinfo ci;
  ci.channel = channel;
  ci.err = 0;
  ci.rssi = rssi;
  ci.header = frame[0];
  ci.last_heard = millis() / 1000;
  ci.reported = 0;
  //int pos = 36;
  uint8_t *bssid;
  uint8_t *station;
  uint8_t *ap;
  uint8_t ds;
  ds = frame[1] & 3;    //Set first 6 bits to 0
  switch (ds) {
    // p[1] - xxxx xx00 => NoDS   p[4]-DST p[10]-SRC p[16]-BSS
    case 0:
      bssid = frame + 16;
      station = frame + 10;
      ap = frame + 4;
      break;
    // p[1] - xxxx xx01 => ToDS   p[4]-BSS p[10]-SRC p[16]-DST
    case 1:
      bssid = frame + 4;
      station = frame + 10;
      ap = frame + 16;
      break;
    // p[1] - xxxx xx10 => FromDS p[4]-DST p[10]-BSS p[16]-SRC
    case 2:
      bssid = frame + 10;
      // hack - don't know why it works like this...
      if (memcmp(frame + 4, broadcast1, 3) || memcmp(frame + 4, broadcast2, 3) || memcmp(frame + 4, broadcast3, 3)) {
        station = frame + 16;
        ap = frame + 4;
      } else {
        station = frame + 4;
        ap = frame + 16;
      }
      break;
    // p[1] - xxxx xx11 => WDS    p[4]-RCV p[10]-TRM p[16]-DST p[26]-SRC
    case 3:
      bssid = frame + 10;
      station = frame + 4;
      ap = frame + 4;
      break;
  }

  memcpy(ci.station, station, ETH_MAC_LEN);
  memcpy(ci.bssid, bssid, ETH_MAC_LEN);
  memcpy(ci.ap, ap, ETH_MAC_LEN);

  ci.seq_n = frame[23] * 0xFF + (frame[22] & 0xF0);
  return ci;
}



struct beaconinfo parse_beacon(uint8_t *frame, uint16_t framelen, signed rssi)
{
  // takes 112 byte beacon frame
  struct beaconinfo bi;
  bi.ssid_len = 0;
  bi.channel = 0;
  bi.err = 0;
  bi.rssi = rssi;
  bi.last_heard = millis() / 1000;
  bi.reported = 0;
  bi.header = frame[0];
  int pos = 36;
  uint8_t frame_type = (frame[0] & 0x0C) >> 2;
  uint8_t frame_subtype = (frame[0] & 0xF0) >> 4;
  if (frame[pos] == 0x00) {
    while (pos < framelen) {
      switch (frame[pos]) {
        case 0x00: //SSID
          bi.ssid_len = (int) frame[pos + 1];
          if (bi.ssid_len == 0) {
            memset(bi.ssid, '\x00', 33);
            break;
          }
          if (bi.ssid_len < 0) {
            bi.err = -1;
            break;
          }
          if (bi.ssid_len > 32) {
            bi.err = -2;
            break;
          }
          memset(bi.ssid, '\x00', 33);
          memcpy(bi.ssid, frame + pos + 2, bi.ssid_len);
          bi.err = 0;  // before was error??
          break;
        case 0x03: //Channel
          bi.channel = (int) frame[pos + 2];
          pos = -1;
          break;
        default:
          break;
      }
      if (pos < 0) break;
      pos += (int) frame[pos + 1] + 2;
    }
  } else {
    bi.err = -3;
  }

  bi.capa[0] = frame[34];
  bi.capa[1] = frame[35];
  memcpy(bi.bssid, frame + 10, ETH_MAC_LEN);
  return bi;
};

struct probeinfo parse_probe(uint8_t *frame, uint16_t framelen, signed rssi)
{
  // takes 112 byte probe request frame
  struct probeinfo pi;
  pi.ssid_len = 0;
  pi.channel = 0;
  pi.err = 0;
  pi.rssi = rssi;
  pi.last_heard = millis() / 1000;
  pi.reported = 0;
  pi.header = frame[0];
  int pos = 24;
  uint8_t frame_type = (frame[0] & 0x0C) >> 2;
  uint8_t frame_subtype = (frame[0] & 0xF0) >> 4;

  if (frame[pos] == 0x00) {
    pi.ssid_len = (int) frame[pos + 1];
    if (pi.ssid_len == 0) {
      memset(pi.ssid, '\x00', 33);
    }
    if (pi.ssid_len < 0) {
      pi.err = -1;
    }
    if (pi.ssid_len > 32) {
      pi.err = -2;
    }
    memset(pi.ssid, '\x00', 33);
    memcpy(pi.ssid, frame + pos + 2, pi.ssid_len);
    pi.err = 0;  // before was error??
  } else {
    pi.err = -3;
  }

  if (pi.err != 0) {
    Serial.printf("Error parsing PROBE %d", (int)pi.err);
  }
  memcpy(pi.ap, frame + 4, ETH_MAC_LEN);
  memcpy(pi.station, frame + 10, ETH_MAC_LEN);
  memcpy(pi.bssid, frame + 16, ETH_MAC_LEN);
  return pi;
}


int register_beacon(beaconinfo beacon)
{
  // add beacon to list if not already included
  int known = 0;   // Clear known flag
  for (int u = 0; u < aps_known_count; u++)
  {
    if (! memcmp(aps_known[u].bssid, beacon.bssid, ETH_MAC_LEN)) {
      known = 1;
      aps_known[u].last_heard = beacon.last_heard;
      break;
    }   // AP known => Set known flag
  }
  if (! known)  // AP is NEW, copy MAC to array and return it
  {
    memcpy(&aps_known[aps_known_count], &beacon, sizeof(beacon));
    aps_known_count++;

    if ((unsigned int) aps_known_count >=
        sizeof (aps_known) / sizeof (aps_known[0]) ) {
      Serial.printf("exceeded max aps_known\n");
      aps_known_count = 0;
    }
  }
  return known;
}

int register_client(clientinfo ci)
{
  // add client to list if not already included
  int known = 0;   // Clear known flag
  for (int u = 0; u < clients_known_count; u++)
  {
    if (! memcmp(clients_known[u].station, ci.station, ETH_MAC_LEN)) {
      known = 1;
      clients_known[u].last_heard = ci.last_heard;
      break;
    }
  }
  if (! known)
  {
    memcpy(&clients_known[clients_known_count], &ci, sizeof(ci));
    clients_known_count++;

    if ((unsigned int) clients_known_count >=
        sizeof (clients_known) / sizeof (clients_known[0]) ) {
      Serial.printf("exceeded max clients_known\n");
      clients_known_count = 0;
    }
  }
  return known;
}

int register_probe(probeinfo pi)
{
  //add probe to list if not already included.
  int known = 0;   // Clear known flag
  for (int u = 0; u < probes_known_count; u++)
  {
    if ((memcmp(probes_known[u].station, pi.station, ETH_MAC_LEN) == 0)
        && (memcmp(probes_known[u].bssid, pi.bssid, ETH_MAC_LEN) == 0 )
        && (strncmp((char*)probes_known[u].ssid, (char*)pi.ssid, sizeof(pi.ssid)) == 0 )) {
      known = 1;
      probes_known[u].last_heard = pi.last_heard;
      break;
    }
  }
  if (! known)
  {
    memcpy(&probes_known[probes_known_count], &pi, sizeof(pi));
    probes_known_count++;

    if ((unsigned int) probes_known_count >=
        sizeof (probes_known) / sizeof (probes_known[0]) ) {
      Serial.printf("exceeded max probes_known\n");
      probes_known_count = 0;
    }
  }
  return known;
}

void print_client(clientinfo ci)
{
  int u = 0;
  int known = 0;   // Clear known flag
  uint64_t now = millis() / 1000;
  if (ci.err != 0) {
    Serial.printf("ci.err %02d", ci.err);
    Serial.printf("\r\n");
  } else {
    Serial.printf("DEVICE: ");
    for (int i = 0; i < 6; i++) Serial.printf("%02x", ci.station[i]);
    Serial.printf(" ==> ");

    for (u = 0; u < aps_known_count; u++)
    {
      if (! memcmp(aps_known[u].bssid, ci.bssid, ETH_MAC_LEN)) {
        Serial.printf("[%32s]  ", aps_known[u].ssid);
        known = 1;     // AP known => Set known flag
        break;
      }
    }

    if (! known)  {
      Serial.printf("[%32s]  ", "??");
    };
    for (int i = 0; i < 6; i++) Serial.printf("%02x", ci.bssid[i]);
    Serial.printf(" %3d", ci.channel);
    Serial.printf("   %d", (now - ci.last_heard));
    Serial.printf("   %d", (ci.reported));
    Serial.printf("   %4d\r\n", ci.rssi);
  }
}


void promisc_cb(uint8_t *buf, uint16_t len)
{
  if (len == 12) {
    struct RxControl *sniffer = (struct RxControl*) buf;
  } else if (len == 128) {
    uint8_t frame_control_pkt = buf[12]; // just after the RxControl Structure
    uint8_t frame_type = (frame_control_pkt & 0x0C) >> 2;
    uint8_t frame_subtype = (frame_control_pkt & 0xF0) >> 4;
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    if (frame_type == 0 && (frame_subtype == 8 || frame_subtype == 5))
    {
      struct beaconinfo beacon = parse_beacon(sniffer->buf, 112, sniffer->rx_ctrl.rssi);
      if (register_beacon(beacon) == 0)
      {
        nothing_new = 0;
      };
    } else if (frame_type == 0 && frame_subtype == 4) {
      struct probeinfo probe = parse_probe(sniffer->buf, 112, sniffer->rx_ctrl.rssi);
      if (register_probe(probe) == 0)
      {
        nothing_new = 0;
      };
    } else {
      // Unknown packet
    };
  } else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
    if (register_client(ci) == 0) {
      print_client(ci);
      nothing_new = 0;
    }
  }
}
