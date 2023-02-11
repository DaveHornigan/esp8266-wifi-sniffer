//
// Created by dave on 09.02.23.
//

#ifndef ESP8266_WIFI_SNIFFER_CONSTANTS_H
#define ESP8266_WIFI_SNIFFER_CONSTANTS_H
#endif //ESP8266_WIFI_SNIFFER_CONSTANTS_H

#define DEBUG_ENABLED 1
#define DEBUG_DISABLED 0
#define DEBUG_MODE DEBUG_ENABLED

#ifdef ESP8266
#define CHIP_ID String(EspClass::getChipId(), HEX)
#elif ESP32
#define CHIP_ID String((uint32_t) ESP.getEfuseMac(), HEX)
#endif

#define AP_WIFI_PASS "0123456789"
#define AP_WIFI_NAME ("WiFi Sniffer - " + CHIP_ID)


