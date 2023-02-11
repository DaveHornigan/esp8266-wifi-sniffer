//
// Created by dave on 09.02.23.
//

#ifndef ESP8266_WIFI_SNIFFER_FUNCTIONS_H
#define ESP8266_WIFI_SNIFFER_FUNCTIONS_H

#ifndef BEACON_DETECTING
#define BEACON_DETECTING 1
#endif

#include <ESP8266WiFi.h>
#include "./structures.h"

// Expose Espressif SDK functionality
extern "C" {
#include "user_interface.h"
typedef void (*freedom_outside_cb_t)(uint8 status);
int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
void wifi_unregister_send_pkt_freedom_cb(void);
int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

#define MAX_APS_TRACKED 10
#define MAX_CLIENTS_TRACKED 30

BeaconInfo aps_known[MAX_APS_TRACKED];                    // Array to save MACs of known APs
int aps_known_count = 0;                                  // Number of known APs
int nothing_new = 0;
ClientInfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs




String formatMac1(uint8_t mac[ETH_MAC_LEN]) {
  String hi = "";
  for (int i = 0; i < ETH_MAC_LEN; i++) {
    if (mac[i] < 16) hi = hi + "0" + String(mac[i], HEX);
    else hi = hi + String(mac[i], HEX);
    if (i < 5) hi = hi + ":";
  }
  return hi;
}

int register_beacon(BeaconInfo beacon) {
  int known = 0;   // Clear known flag
  for (int u = 0; u < aps_known_count; u++) {
    if (!memcmp(aps_known[u].bssid, beacon.bssid, ETH_MAC_LEN)) {
      aps_known[u].lastDiscoveredTime = (long) millis();
      aps_known[u].rssi = beacon.rssi;
      known = 1;
      break;
    }   // AP known => Set known flag
  }
  if (!known && (beacon.err == 0))  // AP is NEW, copy MAC to array and return it
  {
    beacon.lastDiscoveredTime = (long) millis();
    memcpy(&aps_known[aps_known_count], &beacon, sizeof(beacon));
    Serial.print("Register Beacon ");
    Serial.print(formatMac1(beacon.bssid));
    Serial.print(" Channel ");
    Serial.print(aps_known[aps_known_count].channel);
    Serial.print(" RSSI ");
    Serial.println(aps_known[aps_known_count].rssi);

    aps_known_count++;

    if ((unsigned int) aps_known_count >=
        sizeof(aps_known) / sizeof(aps_known[0])) {
      Serial.printf("exceeded max aps_known\n");
      aps_known_count = 0;
    }
  }
  return known;
}

int register_client(ClientInfo &ci) {
  int known = 0;   // Clear known flag
  for (int u = 0; u < clients_known_count; u++) {
    if (!memcmp(clients_known[u].station, ci.station, ETH_MAC_LEN)) {
      clients_known[u].lastDiscoveredTime = (long) millis();
      clients_known[u].rssi = ci.rssi;
      known = 1;
      break;
    }
  }

  //Uncomment the line below to disable collection of probe requests from randomised MAC's
  //if (ci.channel == -2) known = 1; // This will disable collection of probe requests from randomised MAC's

  if (!known) {
    ci.lastDiscoveredTime = (long) millis();
    // search for Assigned AP
    for (int u = 0; u < aps_known_count; u++) {
      if (!memcmp(aps_known[u].bssid, ci.bssid, ETH_MAC_LEN)) {
        ci.channel = aps_known[u].channel;
        break;
      }
    }
    if (ci.channel != 0) {
      memcpy(&clients_known[clients_known_count], &ci, sizeof(ci));

      clients_known_count++;
    }

    if ((unsigned int) clients_known_count >=
        sizeof(clients_known) / sizeof(clients_known[0])) {
      Serial.printf("exceeded max clients_known\n");
      clients_known_count = 0;
    }
  }
  return known;
}

void promisc_cb(uint8_t *buf, uint16_t len) {
  if (len == 12) {
    // nothing
  } else if (len == 128) {
    auto *sniffer = (struct SnifferBuffer2 *) buf;
    if (sniffer->buf[0] == 0x80) {
      #if BEACON_DETECTING == 1
      struct beaconinfo beacon = parse_beacon(sniffer->buf, 112, sniffer->rx_ctrl.rssi);
      if (register_beacon(beacon) == 0) {
        print_beacon(beacon);
        nothing_new = 0;
      }
      #endif
    } else if (sniffer->buf[0] == 0x40) {
      struct ClientInfo ci = parse_probe(sniffer->buf, 36, sniffer->rx_ctrl.rssi);
      if (register_client(ci) == 0) {
        nothing_new = 0;
      }
    }
  } else {
    auto *sniffer = (struct sniffer_buf *) buf;
    //Is data or QOS?
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
      struct ClientInfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN) != 0) {
        if (register_client(ci) == 0) {
          nothing_new = 0;
        }
      }
    }
  }
}

#endif //ESP8266_WIFI_SNIFFER_FUNCTIONS_H
