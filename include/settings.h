//
// Created by dave on 10.02.23.
//

#ifndef ESP8266_WIFI_SNIFFER_SETTINGS_H
#define ESP8266_WIFI_SNIFFER_SETTINGS_H

#define IOTWEBCONF_DEBUG_PWD_TO_SERIAL

#include "IotWebConfMultipleWifi.h"
#include <DNSServer.h>
#include "defines.h"
#include "IotWebConfUsing.h"
#include "PubSubClient.h"

#define BEACON_DETECTING 0

#define SETTING_PARAM_LENGTH 60

bool needMqttConnect = false;
bool needReboot = false;
bool needWiFiConnect = false;
unsigned long lastMqttConnectionAttempt = 0;

// <Settings>
char mqttServer[SETTING_PARAM_LENGTH];
const char* mqttTopic = "hornigan/devices/sniffer";
char mqttUser[SETTING_PARAM_LENGTH];
char mqttPass[SETTING_PARAM_LENGTH];
// </Settings>


DNSServer dnsServer;
WebServer webServer(80);
IotWebConf iotWebConf(AP_WIFI_NAME.c_str(), &dnsServer, &webServer, AP_WIFI_PASS, "1");
WiFiClient wiFiClient;
PubSubClient mqttClient(wiFiClient);

IotWebConfParameterGroup mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT");
IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter(
    "MQTT Server",
    "mqttServer",
    mqttServer,
    SETTING_PARAM_LENGTH,
    "192.168.0.2"
);
IotWebConfTextParameter mqttUserNameParam = IotWebConfTextParameter(
    "MQTT Username",
    "mqttUser",
    mqttUser,
    SETTING_PARAM_LENGTH,
    "mqtt"
);
IotWebConfPasswordParameter mqttPasswordParam = IotWebConfPasswordParameter(
    "MQTT Password",
    "mqttPass",
    mqttPass,
    SETTING_PARAM_LENGTH,
    ""
);

bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper) {
  uint serverLength = webRequestWrapper->arg(mqttServerParam.getId()).length();
  bool hasErrors = false;
  if (serverLength == 0) {
    mqttServerParam.errorMessage = "Please provide an MQTT server";
    hasErrors = true;
  }
  uint ssidLength = iotWebConf.getWifiSsidParameter()->getLength();
  if (ssidLength == 0) {
    iotWebConf.getWifiSsidParameter()->errorMessage = "Please provide SSID";
    hasErrors = true;
  }
  return !hasErrors;
}

void mqttSubscribe() {
//  mqttClient.subscribe("");
}

void mqttMessageReceived(const char *topic, byte *payload, unsigned int length) {
  char message[MQTT_MAX_PACKET_SIZE];
  if (length > 0) {
    memset(message, '\0', sizeof(message));
    memcpy(message, payload, length);
    #if DEBUG_MODE == DEBUG_ENABLED
    Serial.println();
    Serial.print(F("Requested ["));
    Serial.print(topic);
    Serial.print(F("] "));
    Serial.println(message);
    #endif
  }
}

bool apConnect(const char* apName, const char* password) {
  return WiFi.softAP(AP_WIFI_NAME, password, 13, 1);
}

bool wifiReconnect() {
  WiFi.mode(WIFI_STA);
  unsigned short attempts = 0;
  while (!WiFi.isConnected()) {
    if (attempts >= 10) {
      needReboot = true;
      return false;
    }
    attempts++;
    Serial.print("Trying reconnect to WiFi... Attempt#");
    Serial.println(attempts);
    WiFi.reconnect();
    delay(5000);
  }

  return true;
}

bool mqttReconnect() {
  delay(1);
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt || !WiFi.isConnected()) {
    return false;
  }
  if (!mqttClient.connected()) {
    Serial.printf("Connecting to MQTT server...\n");
    mqttClient.setCallback(mqttMessageReceived);
    mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqttClient.setServer(mqttServer, 1883);
    if (mqttClient.connect(
        AP_WIFI_NAME.c_str(),
        mqttUser,
        mqttPass
    )) {
      Serial.println(F("MQTT Connected"));
      mqttSubscribe();
    } else {
      lastMqttConnectionAttempt = now;
      Serial.print("MQTT connect failed with state ");
      Serial.println(mqttClient.state());

      return false;
    }
  }

  return true;
}

void wifiConnected() {
#if DEBUG_MODE == DEBUG_ENABLED
  Serial.println();
  Serial.println(F("=================================================================="));
  Serial.println(F("  Hornigan Wi-Fi Sniffer"));
  Serial.println(F("=================================================================="));
  Serial.println();
  Serial.print(F("Connected to Wifi \t["));
  Serial.print(WiFi.localIP());
  Serial.println(F("]"));
#endif
  needMqttConnect = true;
}

void configSaved() {
  Serial.println("Configuration was updated.");
  needReboot = true;
}

void setupDefaultSettingsValues() {
  iotWebConf.getWifiSsidParameter()->defaultValue = "hornigan.com";
  iotWebConf.getWifiSsidParameter()->applyDefaultValue();
  iotWebConf.getThingNameParameter()->defaultValue = "Hornigan WiFi Sniffer";
  iotWebConf.getThingNameParameter()->applyDefaultValue();
  mqttServerParam.defaultValue = "192.168.0.2";
  mqttServerParam.applyDefaultValue();
  mqttUserNameParam.defaultValue = "mqtt";
  mqttUserNameParam.applyDefaultValue();
}

void setupConfigurationServer() {
  setupDefaultSettingsValues();
  mqttGroup.addItem(&mqttServerParam);
  mqttGroup.addItem(&mqttUserNameParam);
  mqttGroup.addItem(&mqttPasswordParam);
  iotWebConf.addParameterGroup(&mqttGroup);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setApConnectionHandler(&apConnect);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.skipApStartup();

  WiFi.setAutoReconnect(false);

  boolean validConfig = iotWebConf.init();
  if (!validConfig) {
    mqttServer[0] = '\0';
    mqttUser[0] = '\0';
    mqttPass[0] = '\0';
  }

  webServer.on("/", [] { iotWebConf.handleConfig(); });
  webServer.onNotFound([] { iotWebConf.handleNotFound(); });
}

#endif //ESP8266_WIFI_SNIFFER_SETTINGS_H
