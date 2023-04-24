#ifndef MAIN_H_
#define MAIN_H_

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>  
#include <ModbusRTUMaster.h>
#include <ArduinoJson.h>
#include <ModbusRTUMaster.h>
// HTTP OTA UPDATE
#include <HTTPClient.h>
#include <HTTPUpdate.h> 

#define OTA_URL "http://8.213.199.53:8000/static/firmware/firmware.bin"

#define WIFI_STA_NAME "@venus"
#define WIFI_STA_PASS "venus2525"
// mqtt server
#define MQTT_SERVER   "8.213.199.53"
#define MQTT_PORT     1883
#define MQTT_USERNAME "pwa-01"
#define MQTT_PASSWORD "pwa01"  
#define V "0.1.00"
#define UPDATE_LED_STATUS   27
#define SEM_PERIOD  200

#define RX2_PIN 16
#define TX2_PIN 17

void start_update();

#endif