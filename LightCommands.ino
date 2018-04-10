

void setPowerOutput(void) {
  #ifdef WEB_DEBUG
  addLog("setPowerOutput");
  #endif
  digitalWrite(D3, HIGH); // power supply
  delay(20);
  digitalWrite(D1, LOW); // TC4469
}

void cutPowerOutput(void) {
  #ifdef WEB_DEBUG
  addLog("cutPowerOutput");
  #endif
  digitalWrite(D1, HIGH); // TC4469
  delay(20);
  digitalWrite(D3, LOW); // power supply
}

/******************************** TOGGLE ONBOARD LED ***********************************/
void toggleOnboardLed(void) {
  if (gOnboardLedState == HIGH) {
    gOnboardLedState = LOW;
  } else {
    gOnboardLedState = HIGH;
  }
  digitalWrite(LED_CARTE, gOnboardLedState); // Turn off the on-board LED
}


/********************************** LED brightness setter**********************************/
void setLedStripBrightness(uint16_t newBrightness, uint8_t index) {

  if (newBrightness > MAX_BRIGHTNESS_ALLOWED)
    newBrightness = MAX_BRIGHTNESS_ALLOWED;

  if (newBrightness > MAX_PWM_COMMAND)
    newBrightness = MAX_PWM_COMMAND;

  if (newBrightness < PWM_COMMANDED_ZERO_UNDER_COMMAND)
    newBrightness = 0;

  analogWrite(LUT_IndexToPin[index], newBrightness);
}




/********************************** START CALLBACK*****************************************/
void lightCommandCallback(char* topic, byte* payload, unsigned int length) {
#ifdef SERIAL_DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
#endif

  char *topicToTokenize = (char*) malloc (strlen(topic) + 1);
  strcpy(topicToTokenize, topic);

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
#ifdef SERIAL_DEBUG  
  Serial.println(message);
#endif

#ifdef WEB_DEBUG
  gSecondLastCommand = gLastCommand;
  gLastCommand = message;
#endif

  /* first, check topic validity. If it is MQTT_PREFIX /room/MQTT_POWER, then go forward */
  char *token;

  /* get the first token */
  token = strtok(topicToTokenize, "/");

  // does it start with MQTT_PREFIX ?
  if (token == NULL ) {
    //no; leave
#ifdef SERIAL_DEBUG      
    Serial.println("No /");
#endif
    return;
  }

  if (strcmp(token, MQTT_PREFIX) != 0) {
    //no; leave
#ifdef SERIAL_DEBUG      
    Serial.println("wrong MQTT prefix");
#endif
    return;
  }
  token = strtok(NULL, "/");
  if (token == NULL) {
#ifdef SERIAL_DEBUG  
    Serial.println("wrong MQTT topic");
#endif
    return;
  }
  uint8_t index = 0;
#ifdef SERIAL_DEBUG  
  Serial.print("looking for ");
  Serial.println(token);
#endif
  // does it name a known room ?
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
#ifdef SERIAL_DEBUG  
    Serial.print("compare to ");
    Serial.println(LUT_IndexToRoomName[index]);
#endif    
    if (strcmp(LUT_IndexToRoomName[index], token) == 0) {
      // found a match, so we break out of it.
      break;
    }
  }

  // if index equals MAX_ROOM_COUNT, we did not find anything, so leave
  if (index == MAX_ROOM_COUNT) {
#ifdef SERIAL_DEBUG  
    Serial.println("Unknown room");
#endif
    return;
  }

  token = strtok(NULL, "/");
  if (token == NULL) {
    #ifdef SERIAL_DEBUG
    Serial.println("wrong MQTT_SET");
    #endif
    return;
  }

  // does it end with MQTT_SET ?
  if (strcmp(token , MQTT_SET) != 0)  {
    #ifdef SERIAL_DEBUG
    Serial.println("Unknown power suffix");
    #endif
    return;
  }

  // there should not be any data left
  token = strtok(NULL, "/");
  if (token != NULL) {
    #ifdef SERIAL_DEBUG
    Serial.println("Topic too long");
    #endif 
    return;
  }

  // tell the Json processor which queue it should use
  // by passing index
  if (!processJson(message, index)) {
    return;
  }
  sendMqttState(index);
}







