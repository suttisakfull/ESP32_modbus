#include <main.h>
enum {
  RUN = 0, 
  UPDATE
};

// MODBUS
ModbusRTUMaster master(Serial2);
uint32_t lastSentTime = 0UL;
const uint32_t baudrate = 9600UL;
uint32_t delayTime = 1000UL; 
uint32_t responseTimeOut = 2000UL;

typedef uint8_t app_state_t;
app_state_t app_state = RUN; 
long lastReconnectAttempt = 0;
// MQTT
WiFiClient ethClient;
PubSubClient mqttClient(ethClient);
const uint64_t  chipid = ESP.getEfuseMac();
const String device_id =  String((uint16_t)(chipid>>32), HEX) + String((uint32_t)chipid, HEX);
const String willTopic = "iot/" + device_id + "/will";
const String willMessage =  "{\"id\": \"" + device_id +"\", \"message\": \"connection lost\"}"; 
const String sensorTopic = "iot/" + device_id + "/sensor";
const String checkInTopic = "iot/" + device_id + "/checkin";
const String outputSetTopic = "iot/" + device_id + "/output/set";
const String outputGetTopic = "iot/" + device_id + "/output/get"; 
const String updateTopic = "iot/" + device_id + "/update";
const String rebootTopic = "iot/" + device_id + "/reboot";

TaskHandle_t taskHandler = NULL; 
bool coil_status[] = { LOW, LOW, LOW, LOW };
SemaphoreHandle_t sem_modbus;

void read_coil (int slave = 1) {
  if (master.isIdle()) {
    // TODO: Modbus master function 
    if (!master.readCoils(slave, 0, 1)) {  // แก้จาก 4 ไปเป็น 1 จะอ่าน สถานะได้
      Serial.println("RTU READ COILS FAILED...");
    }
  }
  // Check available responses often
  if (master.isWaitingResponse()) {
    ModbusResponse response = master.available();
    uint32_t current = millis();
    while(!response) {
      if (millis() - current > responseTimeOut) {
        Serial.println("Read response timeout");
        break;
      }
      response = master.available();
      delay(20);
    }
    if (response) {
      if (response.hasError()) {
        Serial.print("Error ");
        Serial.println(response.getErrorCode());
      } else {
        // Get the coil value from the response
        Serial.print("RTU Coils values: ");
        for (int i = 0;i<4;i++) {
          coil_status[i] = response.isCoilSet(i);
          Serial.print(coil_status[i]);
          Serial.print(","); 
        }
        Serial.println();
      }
    }
  }  
}

void _digitalWrite(uint8_t addr, uint8_t state) {
  if (master.isIdle()) {
    // TODO: Modbus master function 
    if (!master.writeSingleCoil(1, addr, state)) {
      Serial.println("RTU READ HOLDING REGISTER FAILED...");
    }
  }
  // Check available responses often
  if (master.isWaitingResponse()) {
    ModbusResponse response = master.available();
    uint32_t current = millis();
    while(!response) {
      if (millis() - current > responseTimeOut) {
        Serial.println("Write response timeout");
        break;
      }
      response = master.available();
      delay(20);
    }
    if (response) {
      if (response.hasError()) {
        Serial.print("Error ");
        Serial.println(response.getErrorCode());
      } else {
        // Get the coil value from the response 
        read_coil();  
      }
    }
  }
}

void device_checkin () { 
  DynamicJsonDocument  doc(512); 
  doc["id"] = device_id; 
  doc["v"] = V;
  doc["status"] = true;
  doc["address"] = WiFi.localIP().toString();
  String json = String();
  serializeJson( doc, json); 
  Serial.println(json);
  Serial.println();
  mqttClient.publish(checkInTopic.c_str(), json.c_str(), false );
}
// void send_data(){
//   DynamicJsonDocument  doc(512); 
//             doc["id"] = device_id; 
//             doc["temp"] = random(0,100);
//             doc["humi"] = random(0,100);
//             // doc["temp"] = temp;
//             // doc["humi"] = humi;
//             String json = String();
//             serializeJson( doc, json); 
//             Serial.println(json);
//             Serial.println();
//             mqttClient.publish(sensorTopic.c_str(), json.c_str(), false );
// }

void send_data () {
  if ( xSemaphoreTake(sem_modbus, SEM_PERIOD) == pdTRUE ){ 
    if (master.isIdle()) {
      // TODO: Modbus master function 
      if (!master.readInputRegisters(4, 1, 2)) {  // slave_id,addstart,addstop
        Serial.println("RTU READ HOLDING REGISTER FAILED...");
      }
    }
    if (master.isWaitingResponse()) {
      ModbusResponse response = master.available();
      uint32_t current = millis();
      while(!response) {
        if (millis() - current > responseTimeOut) {
          Serial.println("response timeout:::");
          break;
        }
        response = master.available();
        delay(20);
      }
      if (response) {
        if (response.hasError()) {
          Serial.print("Error ");
          Serial.println(response.getErrorCode());
        } else { 
          float temp = (float)response.getRegister(0) / 10;
          float humi = (float)response.getRegister(1) / 10;
          Serial.println(String(temp)+", "+String(humi));
          if (mqttClient.connected()) {
            DynamicJsonDocument  doc(512); 
            doc["id"] = device_id; 
            // doc["temp"] = random(0,100);
            // doc["humi"] = random(0,100);
            doc["temp"] = temp;
            doc["humi"] = humi;
            String json = String();
            serializeJson( doc, json); 
            Serial.println("-------------------");
            Serial.println(json);
             Serial.println("-------------------");
            Serial.println();
            mqttClient.publish(sensorTopic.c_str(), json.c_str(), false );
          }
        }
      }
    }
    xSemaphoreGive(sem_modbus);
  }
    
}

void vTaskSensor (void * pvParameters) {
  unsigned long lastMs = 0;
  while (true) { 
    unsigned long now = millis();
    if (now - lastMs > 20000) {
      lastMs = now;
      send_data ();
    } 
  }
}

void subscribe () {
  Serial.println("subscribe");
  mqttClient.subscribe(outputSetTopic.c_str());
  mqttClient.subscribe(outputGetTopic.c_str());
  mqttClient.subscribe(updateTopic.c_str());
  mqttClient.subscribe(rebootTopic.c_str());
}

void vTaskStartMqtt(void * pvParameters) {
  Serial.println("MqttClient::vTaskStartMqtt");
  while (true) {
    if (mqttClient.connect(device_id.c_str(), MQTT_USERNAME, MQTT_PASSWORD, willTopic.c_str(), 0, false, willMessage.c_str())) {
      Serial.println("MqttClient::Connected");
      subscribe();
      device_checkin(); 
      break;
    } 
    vTaskDelay(2000/portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void output_get (String topic) {
    Serial.println("Get output state");
    DynamicJsonDocument  doc(512); 
    doc["id"] = device_id;  
    doc["io1"] = coil_status[0];
    doc["io2"] = coil_status[1];
    doc["io3"] = coil_status[2];
    doc["io4"] = coil_status[3];
    String json = String();
    serializeJson( doc, json); 
    Serial.println(json);
    Serial.println();
    String resultTopic = topic + "/result";
    mqttClient.publish(resultTopic.c_str(), json.c_str(), false );
}

void callback(char* topic, byte* payload, unsigned int length) {
  String _topic = String(topic);
  String _payload = String((char*)payload).substring(0, length);
  Serial.println(_payload);
  // restart
  if (rebootTopic.equals(_topic)) {
    ESP.restart();
  }

  if (outputSetTopic.equals(_topic)) {
    Serial.println("Set output state");
    DynamicJsonDocument doc(128);  
    deserializeJson(doc, payload);
    int gpioNumber = doc["index"].as<int32_t>();
    int state  = doc["state"].as<int32_t>();
    if ( xSemaphoreTake(sem_modbus, SEM_PERIOD) == pdTRUE ){
      _digitalWrite(gpioNumber, state);
      output_get(outputSetTopic);
      xSemaphoreGive(sem_modbus);
    }
  }

  if (outputGetTopic.equals(_topic)) {
    if ( xSemaphoreTake(sem_modbus, SEM_PERIOD) == pdTRUE ){
      read_coil();
      output_get(outputGetTopic);
      xSemaphoreGive(sem_modbus);
    }
  }

  if (updateTopic.equals(_topic)) {
    app_state = UPDATE; 
    vTaskSuspend(taskHandler);
    String _updateResultTopic = "iot/" +device_id + "/update/begin"; 
    String playload = "{\"id\": \"" + device_id +"\"}";
    mqttClient.publish(_updateResultTopic.c_str(), playload.c_str(), false );
  }
}

void mqtt_start() {
  xTaskCreatePinnedToCore(vTaskStartMqtt, "vTaskStartMqtt", 1024 * 3, NULL, tskIDLE_PRIORITY, NULL, CONFIG_ARDUINO_RUNNING_CORE);
}

void mqtt_init() {
  Serial.println("Init mqtt clicnt"); 

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT); 
  mqttClient.setKeepAlive(5);
  mqttClient.setCallback(callback);
  mqtt_start();
  if (taskHandler == NULL) {
    xTaskCreatePinnedToCore(vTaskSensor, "vTaskSensor", 1024 * 6, NULL, tskIDLE_PRIORITY, &taskHandler, CONFIG_ARDUINO_RUNNING_CORE);
  }
}

/*void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
    case SYSTEM_EVENT_STA_START: 
      Serial.println("WiFi sta begin");
      break;
    case SYSTEM_EVENT_WIFI_READY:
      Serial.println("WiFi ready");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      Serial.println("WiFi scan done");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi got ip address");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());  
      mqtt_init();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");  
      ESP.restart();
      break;
    }
}*/

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP!");
  Serial.print("SSID Length: ");
  Serial.println(info.connected.ssid_len);
 
  Serial.print("SSID: ");
  for(int i=0; i<info.connected.ssid_len; i++){
    Serial.print((char) info.connected.ssid[i]);
  }
 
  Serial.print("\nBSSID: ");
  for(int i=0; i<6; i++){
    Serial.printf("%02X", info.connected.bssid[i]);
    if(i<5){
        Serial.print(":");
    }
  } 
  Serial.print("\nChannel: ");
  Serial.println(info.connected.channel);
 
  Serial.print("Auth mode: ");
  Serial.println(info.connected.authmode); 

  Serial.println(WiFi.localIP());
}

void WiFiStationGotIp(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi got ip address");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 
  mqtt_init();   
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(baudrate, SERIAL_8N1, RX2_PIN, TX2_PIN); 
  master.begin(baudrate);
  sem_modbus = xSemaphoreCreateMutex();
  read_coil(); 
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED); 
  WiFi.onEvent(WiFiStationGotIp, SYSTEM_EVENT_STA_GOT_IP); 
  WiFi.begin(WIFI_STA_NAME, WIFI_STA_PASS);

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 
  Serial.print("Device_id:");
  Serial.println(device_id);
}

void loop() {
  switch (app_state) {
    case RUN: 
      if (!mqttClient.connected()) {
        long now = millis();
        if (now - lastReconnectAttempt > 5000) {
          lastReconnectAttempt = now;
          Serial.println("Restart mqtt client...");
          mqtt_start();
        }
      } else { 
        mqttClient.loop();
      }
      break;
    case UPDATE:
      Serial.println("Start firmware update");
      start_update();
      break;
  }
  
  delay(10);
}