/********************************** START RECONNECT*****************************************/
void reconnectMqtt() {
  // Loop until we're reconnected
  while (!gMqttClient.connected()) {
#ifdef SERIAL_DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
// Attempt to connect
    if (gMqttClient.connect(DEVICENAME, gConfMqttUsername, gConfMqttPassword)) {
#ifdef SERIAL_DEBUG
      Serial.println("connected");
#endif
      /* subscribe to the MAX_ROOM_COUNT set topics */
      uint8_t index = 0;
      for (index = 0; index < MAX_ROOM_COUNT; index++) {
        gMqttClient.subscribe(gLedStrip[index].setTopic);
        sendMqttState(index);
      }
    } else {
#ifdef SERIAL_DEBUG      
      Serial.print("failed, rc=");
      Serial.print(gMqttClient.state());
      Serial.println(" try again in 5 seconds");
#endif      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/********************************** START SEND STATE*****************************************/
void sendMqttState(uint8_t index) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
  root["CurrentBrightness"] = gLedStrip[index].currentBrightness;
  if (gpCurrentTransition[index] != NULL) {
    root["transition][targetBrightness"] = gpCurrentTransition[index]->targetBrightness;
    root["transition][stepBrightness"] = gpCurrentTransition[index]->stepBrightness;
    root["transition][stepDelay"] = gpCurrentTransition[index]->stepDelay;
    root["transition][stepCount"] = gpCurrentTransition[index]->stepCount;
  }

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  gMqttClient.publish(gLedStrip[index].stateTopic, buffer, true);
}
