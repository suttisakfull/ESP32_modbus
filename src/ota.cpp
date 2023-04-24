#include <main.h>


void start_update() {
  if((WiFi.status() == WL_CONNECTED)) {
    Serial.println("Begin update firmware");
    WiFiClient client; 
    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    httpUpdate.setLedPin(UPDATE_LED_STATUS);
    httpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return ret = httpUpdate.update(client, OTA_URL); 
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str()); 
        delay(3000);
        ESP.restart();
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES"); 
        delay(3000);
        ESP.restart();
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK"); 
        digitalWrite(UPDATE_LED_STATUS, HIGH);
        delay(3000);
        digitalWrite(UPDATE_LED_STATUS, LOW);
        delay(1000);
        ESP.restart();
        break;
    }
  }  
}
