/*
  The code was first based on @corbanmailloux work ( https://github.com/corbanmailloux/esp-mqtt-rgb-led ), now only the wireless and OTA part remains.
  
  To use this code you will need the following dependancies: 
  
  - Support for the ESP8266 boards. 
        - You can add it to the board manager by going to File -> Preference and pasting http://arduino.esp8266.com/stable/package_esp8266com_index.json into the Additional Board Managers URL field.
        - Next, download the ESP8266 dependancies by going to Tools -> Board -> Board Manager and searching for ESP8266 and installing it.
  
  - You will also need to download the follow libraries by going to Sketch -> Include Libraries -> Manage Libraries
      - PubSubClient
      - ArduinoJSON
*/

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include "settings.h"
#include "globalVars.h"


/******************************** TOGGLE ONBOARD LED ***********************************/
void toggleOnboardLed(void) {
  if (gOnboardLedState == HIGH) {
    gOnboardLedState = LOW;
  } else {
    gOnboardLedState = HIGH;
  }
  digitalWrite(LED_CARTE, gOnboardLedState); // Turn off the on-board LED
}

/********************************** PIN SETUP   ****************************************/
void pinSetup() {
// set all pins to output mode and state except D1
  pinMode(D0,OUTPUT);         // pwm 1
  analogWrite(D0,0);
  pinMode(D1,OUTPUT);         // enable pwm out. 
  digitalWrite(D1,HIGH);      // this is connected to a NOT AND pin, so set it high at startup
  pinMode(D2,OUTPUT);         // pwm 4
  analogWrite(D2,0);
  pinMode(D3,OUTPUT);         // power on demand
  digitalWrite(D3,LOW);       // set it to NO POWER on startup
  pinMode(D4,OUTPUT);         // not used
  digitalWrite(D4,LOW);
  pinMode(D5,OUTPUT);         // pwm 2
  analogWrite(D5,0);
  pinMode(D6,OUTPUT);         // pwm 3
  analogWrite(D6,0);
  pinMode(D7,OUTPUT);         // not used
  digitalWrite(D7,LOW);
  pinMode(D8,OUTPUT);         // not used
  digitalWrite(D8,LOW);
  
  // onboard led
  pinMode(LED_CARTE, OUTPUT);
}  

void setPowerOutput(void) {
  digitalWrite(D3,HIGH); // power supply
  digitalWrite(D1,LOW);  // TC4469
}

void cutPowerOutput(void) {
  digitalWrite(D3,LOW);  // power supply
  digitalWrite(D1,HIGH); // TC4469
}


/********************************** START SETUP ****************************************/
void setup() {

  // serial
  Serial.begin(115200);

  //wifi not starting fix
  WiFi.mode(WIFI_OFF);
  toggleOnboardLed();

  // setup pins
  pinSetup();
  
  // Init scenario queues
  int index=0;
  size_t topicSize = 0;
  for(index=0;index<MAX_ROOM_COUNT;index++) {
    STAILQ_INIT(&(gLedStrip[index].transitionQueue));
    /* init MQTT set and state topic: get the size first. do not forget trailing \0 and the 2 slashes */
    topicSize = strlen (MQTT_PREFIX) + strlen (LUT_IndexToRoomName[index]) + strlen (MQTT_STATE) + 3; 
    gLedStrip[index].stateTopic = (char *)malloc(topicSize);
    sprintf(gLedStrip[index].stateTopic,"%s/%s/%s",MQTT_PREFIX,LUT_IndexToRoomName[index],MQTT_STATE);

    topicSize = strlen (MQTT_PREFIX) + strlen (LUT_IndexToRoomName[index]) + strlen (MQTT_SET) + 3; 
    gLedStrip[index].setTopic = (char *)malloc(topicSize);
    sprintf(gLedStrip[index].setTopic,"%s/%s/%s",MQTT_PREFIX,LUT_IndexToRoomName[index],MQTT_SET);
  }
  
  byte received= 0;
  Serial.println("Press S for serial mode, T for test mode");
  delay(500);
  for(index=0;(index<10) && (!gSerialMode);index++) {
    while (Serial.available()) {
      received = Serial.read();
      if (received == 'S') {
        gSerialMode = true;
        break;
      } else if (received == 'T') {
        gSerialMode = true;
        gSpecialSerialMode = true;
        break;
      }
    }
    Serial.print(".");
    delay(500);      
  }

 if (gSpecialSerialMode) {
  Serial.println("Special Serial Mode engaged"); 
 }
 else if (gSerialMode) {
  Serial.println("Serial Mode engaged"); 
 } else {
  
   // wifi setup
  setup_wifi();

/******************************* NTP management **************************************/
  // NTP and time setup.
  // this one sets the NTP server as gConftimeServer with gmt+1
  // and summer/winter mechanic (true)
  NTP.begin(gConftimeServer, 1, true);
  // set the system to do a sync every 86400 seconds when succesful, so once a day 
  // if not successful, try syncing every 600s, so every 10 mins
  NTP.setInterval(600,86400);  

  NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
  if (error) {
    Serial.print("Time Sync error: ");
    if (error == noResponse) {
      Serial.println("NTP server not reachable");
    } else if (error == invalidAddress) {
      Serial.println("Invalid NTP server address");
    }
  } else {
    setTime(NTP.getLastNTPSync());
    Serial.print("Setting time to ");
    Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
  }
});
  
  // MQTT setup
  Serial.println("Wifi connected, reaching for MQTT server"); 
  gMqttClient.setServer(gConfMqttServerIp, gConfMqttPort);
  gMqttClient.setCallback(lightCommandCallback);

  //OTA setup
  ArduinoOTA.setPort(OTAPORT);
  // Hostname defaults to esp8266-[ChgConfIpAddressID]
  ArduinoOTA.setHostname(DEVICENAME);

  // No authentication by default
  ArduinoOTA.setPassword(OTAPASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  }
}


/********************************** START SETUP WIFI*****************************************/
void setup_wifi(void) {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(gConfSsid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(gConfSsid, gConfPassword);
  delay(50);
  wl_status_t retCon = WiFi.status() ;
  
  while (retCon != WL_CONNECTED) {
    WiFi.printDiag(Serial);
    Serial.print("Wifi status: ");
    switch (retCon) {
      case  WL_CONNECTED: Serial.println(" WL_CONNECTED"); break;
      case  WL_NO_SSID_AVAIL: Serial.println(" WL_NO_SSID_AVAIL"); break;
      case  WL_CONNECT_FAILED: Serial.println(" WL_CONNECT_FAILED (maybe wrong pwd)"); break;
      case  WL_IDLE_STATUS: Serial.println(" WL_IDLE_STATUS"); break;
      default: Serial.println("WL_DISCONNECTED"); break;
    }
    delay(1000);
    toggleOnboardLed();
    retCon = WiFi.status() ;
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}



/*
  SAMPLE PAYLOADS:
  {
    "state": "ON"
  }

  {
    "state": "ON" ,
    "brightness": 600,
  }
  
  {
    "state": "ON"
    "brightness": 600,
    "transition" : { "preset": "AUTO" }
  }

  
  {
    "state": "ON"
    "transition" : { "preset": "CLOCK", "TimeMs": 2000 }
  }

  
  {
    "state": "ON"
    "brightness": 600,
    "transition" : { "preset": "CLOCK", "TimeMs": 2000 }
  }

*/



/********************************** START CALLBACK*****************************************/
void lightCommandCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  /* first, check topic validity. If it is MQTT_PREFIX /room/MQTT_POWER, then go forward */
  char *token;
   
  /* get the first token */
  token = strtok(topic, "/");

   // does it start with MQTT_PREFIX ?
  if ((token != NULL ) && strcmp(token,MQTT_PREFIX) != 0) {
    //no; leave
    Serial.println("wrong MQTT prefix");
    return;
  }
  token = strtok(NULL, topic);
  uint8_t index = 0;
  // does it name a known room ?
  for (index=0; index < MAX_ROOM_COUNT; index++) {
    if (strcmp(LUT_IndexToRoomName[index], token) == 0) {
       // found a match, so we break out of it.
      break;
    }
  }

  // if index equals MAX_ROOM_COUNT, we did not find anything, so leave
  if (index == MAX_ROOM_COUNT) {
    Serial.println("Unknown room");
    return;
  }

  token = strtok(NULL, topic);
  // does it end with MQTT_POWER ?
  if (strcmp(token , MQTT_SET) != 0)  {
    Serial.println("Unknown power suffix");
    return;
  }

  // there should not be any data left
  token = strtok(NULL, topic);
  if (token != NULL) {
    Serial.println("Topic too long");
    return;
  }

  // tell the Json processor which queue it should use
  // by passing index
  if (!processJson(message, index)) {
    return;
  }
 
  sendState(index);
}

/****************** scenario builder from JSON *******************************************/
void buildAndInsertScenario (int16_t targetBrightness,uint16_t transitionTimeMs, uint8_t transitionPreset, uint8_t index) {
 
  // TODO 
  // right now the preset is not used. Code it when time and light sensor is available
  switch (transitionPreset) {
    
    case CLOCK_BASED: /* this is the clock based mode */
      /* in clock based mode, the more it gets near a set hour,
      the dimmer the light,
      the longer the transition */
      break;
      
    case LIGHT_BASED: /* this is the light based mode */
      break;

    case CLOCK_AND_LIGHT_BASED: /*  this is the clock and light based mode */
      break;

    default: /* this is the manual mode */
      break;
  }
  
  
  Serial.print("Scenario fading from ");
  Serial.print(gLedStrip[index].currentBrightness,DEC);
  Serial.print(" to ");
  Serial.print(targetBrightness,DEC);
  Serial.print(" in ");
  Serial.print(transitionTimeMs,DEC);
  Serial.print(" ms with preset: ");
  Serial.println(transitionPreset, DEC);

  
  // we want the max stepCount we can, so it depends wether transition time 
  // or brightness count is higher. 
  // we might want to set a minimum delay or minimum brightness step
  // so account for that
  transitionQueueElem_t *pTransition = (transitionQueueElem_t *)malloc(sizeof(transitionQueueElem_t));
  
  if ((abs(targetBrightness - gLedStrip[index].currentBrightness) / MIN_STEP_BRIGHTNESS) > (transitionTimeMs / MIN_STEP_DELAY)) {
  //we can do more than 1 brightness step per delay step ms. set stepDelay to that time
    pTransition->stepDelay = MIN_STEP_DELAY;
    pTransition->stepCount = transitionTimeMs / pTransition->stepDelay;
    pTransition->stepBrightness = (targetBrightness - gLedStrip[index].currentBrightness) / pTransition->stepCount;
  } else {
  //we cannot. brightness difference is the limiting factor.
    pTransition->stepCount = abs(targetBrightness - gLedStrip[index].currentBrightness) / MIN_STEP_BRIGHTNESS;
    pTransition->stepDelay = transitionTimeMs / pTransition->stepCount ;
    pTransition->stepBrightness = (targetBrightness > gLedStrip[index].currentBrightness)?MIN_STEP_BRIGHTNESS:-MIN_STEP_BRIGHTNESS;
  }

  STAILQ_INSERT_HEAD(&(gLedStrip[index].transitionQueue),pTransition,transitionEntry);

  Serial.print("delay step ");
  Serial.print(pTransition->stepDelay,DEC);
  Serial.print(", step count ");
  Serial.print(pTransition->stepCount,DEC);
  Serial.print(", brightness step ");
  Serial.println(pTransition->stepBrightness,DEC);
}

/********************************** JSON PROCESSOR *****************************************/
bool processJson(char* message, uint8_t index) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }
  
  uint16_t targetBrightness = 0;
  uint16_t transitionTimeMs = MIN_TRANSITION_TIME ;
  uint8_t transitionPreset = MANUAL;

  /* The "state" command is self sufficient. If it is present, 
     there is no need to parse anything else.
     Other data is relevant when state is not in the JSON . We still need to set 
     targetBrightness, transitionTimeMs and transitionPreset */
  
  const char* res = root["state"];
  if (res) {
    if (strcmp(root["state"], gConfOnCommand) == 0 ) {
      Serial.println("State set ON");
      targetBrightness = MAX_BRIGHTNESS_ALLOWED;
      transitionPreset = CLOCK_BASED;
    } else {
      Serial.println("State set and not ON, setting as OFF");
      transitionPreset = CLOCK_BASED;
    } 
  } else { // if there is no STATE in the JSON
    res = root["transition"];
    if (!res) {
      Serial.println("No transition set");  
      transitionPreset= CLOCK_BASED;
      transitionTimeMs = MIN_TRANSITION_TIME;
    } else {
      res = root["transition"]["timeMs"];
      if (res) {
        if (root["transition"]["timeMs"] <= MIN_TRANSITION_TIME) {
          Serial.println("Transition too short, setting to lower limit");  
          transitionTimeMs = MIN_TRANSITION_TIME;
        } else if (root["transition"]["timeMs"] > MAX_TRANSITION_TIME) { 
          Serial.println("Transition too short high, setting to higher limit");  
          transitionTimeMs = MAX_TRANSITION_TIME;
        } else {
          transitionTimeMs = root["transition"]["timeMs"];
        }
      } else {
        Serial.println("Transition time not set, setting to lower limit");  
        transitionTimeMs = MIN_TRANSITION_TIME;
      }

      res = root["transition"]["preset"];
      if (res) {
        if (strcmp(root["transition"]["preset"], "CLOCK") == 0) {
        transitionPreset= CLOCK_BASED;
        Serial.println("Transition clock based");  
      } else if (strcmp(root["transition"]["preset"], "LIGHT") == 0) {
        Serial.println("Transition light sensor based");  
        transitionPreset= LIGHT_BASED;
      } else if (strcmp(root["transition"]["preset"], "AUTO") == 0) {
        Serial.println("Transition both clock and light sensor based");  
        transitionPreset= CLOCK_AND_LIGHT_BASED;
      } else {
        Serial.println("Transition set to manual");  
        transitionPreset= MANUAL;
      }
    } else {
      Serial.println("Transition not specified");  
      transitionPreset= MANUAL;
    }
   } // if there is transition in the JSON

   res = root["brightness"];
   if (res) {
     if (root["brightness"] > MAX_BRIGHTNESS_ALLOWED) {
        Serial.println("Brightness set too high, limiting");
        targetBrightness = MAX_BRIGHTNESS_ALLOWED;
       } else {
        targetBrightness = root["brightness"];
       }
    } else { // if there is brightness in the json
      // if not specified, root brightness is 0
        Serial.print("No brightness specified");
        targetBrightness = 0;
        Serial.println(" set to 0");
    } // if there is no brightness in the json
  }
  
  // free the data in the queue 
  transitionQueueElem_t *pTransition = NULL;
  STAILQ_FOREACH(pTransition, &(gLedStrip[index].transitionQueue), transitionEntry) {
    STAILQ_REMOVE_HEAD(&(gLedStrip[index].transitionQueue), transitionEntry);
    free(pTransition);
  }

  //build and insert the scenario
  buildAndInsertScenario(targetBrightness, transitionTimeMs, transitionPreset, index);

  return true;
}


/********************************** START SEND STATE*****************************************/
void sendState(uint8_t index) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
    root["CurrentBrightness"] = gLedStrip[index].currentBrightness;
    if (gpCurrentTransition!= NULL) {
      root["transition][stepBrightness"] = gpCurrentTransition[index]->stepBrightness;
      root["transition][stepDelay"] = gpCurrentTransition[index]->stepDelay;
      root["transition][stepCount"] = gpCurrentTransition[index]->stepCount;

  }
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  
  gMqttClient.publish(gLedStrip[index].stateTopic, buffer, true);
}

/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!gMqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (gMqttClient.connect(DEVICENAME, gConfMqttUsername, gConfMqttPassword)) {
      Serial.println("connected");

       /* subscribe to the MAX_ROOM_COUNT set topics */
      uint8_t index = 0;
      for(index=0;index< MAX_ROOM_COUNT;index++) {
        gMqttClient.subscribe(gLedStrip[index].setTopic);
        sendState(index);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(gMqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


/********************************** LED brightness setter**********************************/
void setLedStripBrightness(uint16_t newBrightness, uint8_t index) {
    
    if (newBrightness > MAX_BRIGHTNESS_ALLOWED)
      newBrightness = MAX_BRIGHTNESS_ALLOWED;
  
    if (newBrightness > MAX_PWM_COMMAND)
      newBrightness = MAX_PWM_COMMAND;
  
    if (newBrightness <PWM_COMMANDED_ZERO_UNDER_COMMAND)
      newBrightness = 0;
  
    analogWrite(LUT_IndexToPin[index],newBrightness);
}

/********************************** START MAIN LOOP*****************************************/
void loop() {
    if (gSpecialSerialMode) {
      if (Serial.available()) {
        while(Serial.available()) {
          if (gSerialBufferPos < (SERIAL_BUFFER_SIZE -1)) {
              gSerialBuffer[gSerialBufferPos] = Serial.read();
              if (gSerialBuffer[gSerialBufferPos] == '+'){
                gSpecialBrightness+=50;
                if (gSpecialBrightness > MAX_PWM_COMMAND)
                  gSpecialBrightness = MAX_PWM_COMMAND;
                setLedStripBrightness(gSpecialBrightness,0);
              } else if (gSerialBuffer[gSerialBufferPos] == '-'){
                gSpecialBrightness-=50;
                if (gSpecialBrightness < PWM_COMMANDED_ZERO_UNDER_COMMAND)
                  gSpecialBrightness = 0 ;
                setLedStripBrightness(gSpecialBrightness,0);
              }                
              gSerialBufferPos = 0;
          }
        }
      }
    } else if (gSerialMode) {
    if (Serial.available()) {
      if (gpCurrentTransition == NULL) {
        while(Serial.available()) {
          if (gSerialBufferPos < (SERIAL_BUFFER_SIZE -1)) {
              gSerialBuffer[gSerialBufferPos] = Serial.read();
              gSerialBufferPos++;
            if ( gSerialBuffer[gSerialBufferPos - 1] == '\n') { 
              lightCommandCallback(gLedStrip[0].setTopic,gSerialBuffer,gSerialBufferPos);
              gSerialBufferPos = 0;
            }
          } else {
            Serial.read();
            gSerialBufferPos = 0;
            break;
          }
        }
      } else {
        Serial.read();
      }
    }
  }
  else {
    if (!gMqttClient.connected()) {
      reconnect();
    }

    if (WiFi.status() != WL_CONNECTED) {
      delay(1);
      Serial.print("WIFI Disconnected. Attempting reconnection.");
      setup_wifi();
      return;
    }

    gMqttClient.loop();

    ArduinoOTA.handle();
  }

  // are we processing a transition ?
  uint8_t transitionIndex = 0;
  bool isPowerStillNeeded = false;
  for (transitionIndex = 0; transitionIndex < MAX_ROOM_COUNT; transitionIndex++) {
    if (gpCurrentTransition[transitionIndex] ==NULL) {
    // no. Load one if available.
    //is there an available transition ?
      if (!STAILQ_EMPTY(&(gLedStrip[transitionIndex].transitionQueue))) {
        // yes, load it
        gpCurrentTransition[transitionIndex] = STAILQ_FIRST(&(gLedStrip[transitionIndex].transitionQueue));
        // and remove it from the queue 
        STAILQ_REMOVE_HEAD(&(gLedStrip[transitionIndex].transitionQueue), transitionEntry);
      }
    }

    //lets check again.
    // are we processing a transition ?
    if (!(gpCurrentTransition[transitionIndex]==NULL)) {
      delay(gpCurrentTransition[transitionIndex]->stepDelay);
      gLedStrip[transitionIndex].currentBrightness += gpCurrentTransition[transitionIndex]->stepBrightness;
      setLedStripBrightness(gLedStrip[transitionIndex].currentBrightness, transitionIndex);
      // did we reach the end ?
      if (gpCurrentTransition[transitionIndex]->stepCount == 0) {
        // yes. Free the transition.
        Serial.println("Stop Fading");
        free(gpCurrentTransition[transitionIndex]);
        gpCurrentTransition[transitionIndex] = NULL;
      } else {
        // no. Decrease step count
        gpCurrentTransition[transitionIndex]->stepCount--;
      }
    } else {
      // no transition available, do nothing. 
    }  
    //We want to check if power is still needed.
    isPowerStillNeeded |= (gLedStrip[transitionIndex].currentBrightness > 0);
  }
  if (isPowerStillNeeded) {
    setPowerOutput();
  } else {
    cutPowerOutput(); 
  }
}

