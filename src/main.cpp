#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "../include/settings.h"
#include "../include/functions.h"

#define disable 0
#define enable  1
#define DETECT_TIME 10000
#define SENDTIME 30000
#define MAXDEVICES 40
#define JBUFFER (MAXDEVICES * 100)
#define PURGETIME 600000
#define MINRSSI (-70)

int clients_known_count_old, aps_known_count_old;
unsigned long sendEntry, detectTime;
char jsonString[JBUFFER];

StaticJsonDocument<JBUFFER> jsonBuffer;

void setup() {
  // https://github.com/prampec/IotWebConf/blob/master/examples/IotWebConf06MqttApp/IotWebConf06MqttApp.ino
  Serial.begin(115200);
  delay(3000);
  Serial.printf("\n\nSDK version:%s\n\r", system_get_sdk_version());

  setupConfigurationServer();
  wifi_set_promiscuous_rx_cb(promisc_cb);
}


void detectDevices();

void purgeDevices();

void showDevices();

void sendDevices();

void loop() {
  iotWebConf.doLoop();
  if (needReboot) {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    EspClass::restart();
  }
  if (needWiFiConnect && iotWebConf.getState() != iotwebconf::NotConfigured) {
    if (!wifiReconnect()) {
      return;
    }
  }
  if (needMqttConnect) {
    needMqttConnect = !mqttReconnect();
  } else if (iotWebConf.getState() == iotwebconf::OnLine && !mqttClient.connected()) {
    mqttReconnect();
  }
  if (millis() - sendEntry > SENDTIME) {
    if (!WiFi.isConnected()) {
      needWiFiConnect = true;
      needMqttConnect = true;
      return;
    } else if (!mqttClient.connected()) {
      needMqttConnect = true;
      return;
    }

    sendDevices();
  }
  if (millis() - detectTime > DETECT_TIME) { detectDevices(); }
  purgeDevices();
}

void detectDevices() {
  if (mqttClient.connected()) { mqttClient.disconnect(); }
  if (WiFi.isConnected()) { WiFi.disconnect(); }

  Serial.println("Start Devices Detecting");
  detectTime = millis();
  wifi_promiscuous_enable(enable);
  for (int channel = 1; channel <= 15; channel++) {
    Serial.print("Scanning channel: ");
    Serial.println(channel);
    wifi_set_channel(channel);
    for (int i = 0; i <= 300; ++i) {
      delay(1); // Wait detecting
    }
  }
  wifi_promiscuous_enable(disable);
  showDevices();
}


void purgeDevices() {
  for (int u = 0; u < clients_known_count; u++) {
    if ((millis() - clients_known[u].lastDiscoveredTime) > PURGETIME) {
      Serial.print("purge Client");
      Serial.println(u);
      for (int i = u; i < clients_known_count; i++)
        memcpy(&clients_known[i], &clients_known[i + 1], sizeof(clients_known[i]));
      clients_known_count--;
      break;
    }
  }
  for (int u = 0; u < aps_known_count; u++) {
    if ((millis() - aps_known[u].lastDiscoveredTime) > PURGETIME) {
      Serial.print("purge Bacon");
      Serial.println(u);
      for (int i = u; i < aps_known_count; i++) memcpy(&aps_known[i], &aps_known[i + 1], sizeof(aps_known[i]));
      aps_known_count--;
      break;
    }
  }
}


void showDevices() {
  Serial.println();
  Serial.println();
  Serial.println("-------------------Device DB-------------------");
  Serial.printf("%4d Devices + Clients.\n", aps_known_count + clients_known_count); // show count

  // show Beacons
  for (int u = 0; u < aps_known_count; u++) {
    Serial.printf("%4d ", u); // Show beacon number
    Serial.print("| Beacon ");
    Serial.print(formatMac1(aps_known[u].bssid));
    Serial.print(" | RSSI ");
    Serial.print(aps_known[u].rssi);
    Serial.print(" | Channel ");
    Serial.println(aps_known[u].channel);
  }

  // show Clients
  for (int u = 0; u < clients_known_count; u++) {
    Serial.printf("%4d ", u); // Show client number
    Serial.print("| Client ");
    Serial.print(formatMac1(clients_known[u].station));
    Serial.print(" | RSSI ");
    Serial.print(clients_known[u].rssi);
    Serial.print(" | Channel ");
    Serial.println(clients_known[u].channel);
  }
}

void sendDevices() {
  String deviceMac;

  // Setup MQTT
  if (!WiFi.isConnected()) { return; }
  if (!mqttClient.connected()) { return; }

  // Purge json string
  jsonBuffer.clear();

  #if BEACON_DETECTING == 1
  // add Beacons
  for (int u = 0; u < aps_known_count; u++) {
    deviceMac = formatMac1(aps_known[u].bssid);
    if (aps_known[u].rssi > MINRSSI) {
      mac.add(deviceMac);
      rssi.add(aps_known[u].rssi);
    }
  }
  #endif

  // Add Clients
  for (int u = 0; u < clients_known_count; u++) {
    deviceMac = formatMac1(clients_known[u].station);
    if (clients_known[u].rssi > MINRSSI) {
      JsonObject device = jsonBuffer.createNestedObject();
      device["MAC"] = deviceMac;
      device["RSSI"] = clients_known[u].rssi;
      device["channel"] = clients_known[u].channel;
    }
  }

  Serial.println();
  Serial.printf("number of devices: %02d\n", jsonBuffer.size());
//  serializeJsonPretty(jsonBuffer, Serial);
  serializeJson(jsonBuffer, jsonString);
  Serial.println();
  //  Serial.println((jsonString));
  if (mqttClient.publish(mqttTopic, jsonString)) {
    Serial.println("Successfully published");
  } else {
    Serial.printf(
        "MQTT write error: %d; Buffer size: %d; Message size: %d",
        mqttClient.getWriteError(),
        mqttClient.getBufferSize(),
        strlen(jsonString)
    );
    Serial.println();
  }
  mqttClient.loop();
  mqttClient.disconnect();
  delay(100);
  sendEntry = millis();
}