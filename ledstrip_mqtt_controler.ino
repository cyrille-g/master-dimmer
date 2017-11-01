/*
  Thanks much to @corbanmailloux for providing a great framework for implementing flash/fade with HomeAssistant https://github.com/corbanmailloux/esp-mqtt-rgb-led
  
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
#include "settings.h"

/******************************** TOGGLE ONBOARD LED ***********************************/
void toggleOnboardLed(void)
{
  if (gOnboardLedState == HIGH)
    gOnboardLedState = LOW;
  else
    gOnboardLedState = HIGH;
    digitalWrite(LED_CARTE, gOnboardLedState); // Turn off the on-board LED
}



/********************************** START SETUP*****************************************/
void setup() {

  // serial
  Serial.begin(115200);

// set all pins to output mode and state low
  pinMode(D0,OUTPUT);
  analogWrite(D0,0);
  pinMode(D1,OUTPUT);
  analogWrite(D1,0);
  pinMode(D2,OUTPUT);
  analogWrite(D2,0);
  pinMode(D3,OUTPUT);
  analogWrite(D3,0);
  pinMode(D4,OUTPUT);
  analogWrite(D4,0);
  pinMode(D5,OUTPUT);
  analogWrite(D5,0);
  pinMode(D6,OUTPUT);
  analogWrite(D6,0);
  pinMode(D7,OUTPUT);
  analogWrite(D7,0);
  pinMode(D8,OUTPUT);
  analogWrite(D8,0);
  
  // onboard led
  pinMode(LED_CARTE, OUTPUT);
  toggleOnboardLed();
  byte received= 0;
  int i=0;
  Serial.println("Press S for serial mode, T for test mode");
  delay(500);
  for(i=0;(i<10) && (!serialMode);i++) {
    while (Serial.available()) {
      received = Serial.read();
      if (received == 'S') {
        serialMode = true;
        break;
      } else if (received == 'T') {
        serialMode = true;
        specialSerialMode = true;
        break;
      }
    }
    Serial.print(".");
    delay(500);      
  }

 if (specialSerialMode) {
  Serial.println("Special Serial Mode engaged"); 
 }
 else if (serialMode) {
  Serial.println("Serial Mode engaged"); 
 } else {
  // wifi
  setup_wifi();
  Serial.println("Wifi connected, reaching for MQTT server"); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(lightCommandCallback);

  //OTA SETUP
  ArduinoOTA.setPort(OTAPORT);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(SENSORNAME);

  // No authentication by default
  ArduinoOTA.setPassword(OTAPASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
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
  currentBrightness = 0;
}


/********************************** START SETUP WIFI*****************************************/
void setup_wifi(void) {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  wl_status_t retCon = WiFi.status() ;
  
  while (retCon != WL_CONNECTED) {
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
void lightCommandCallback(const char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processJson(message)) {
    return;
  }
 
  sendState();
}



/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  stateOn = (strcmp(root["state"], on_cmd) == 0);
  if  (stateOn)
    Serial.println("State ON");
  else 
    Serial.println("State OFF or not set, setting as OFF");
  
  transitionTimeMs = root["transition"]["timeMs"];

  if (transitionTimeMs <= MIN_TRANSITION_TIME) {
    Serial.println("Transition too short or not set, setting to lower limit");  
    transitionTimeMs = MIN_TRANSITION_TIME;
  }

  if (strcmp(root["transition"]["preset"], "CLOCK") == 0) {
    transitionPreset= CLOCK_BASED;
    Serial.println("Transition clock based");  
  } else if (strcmp(root["transition"]["mode"], "LIGHT") == 0) {
    Serial.println("Transition light sensor based");  
    transitionPreset= LIGHT_BASED;
  } else if (strcmp(root["transition"]["mode"], "AUTO") == 0) {
    Serial.println("Transition both clock and light sensor based");  
    transitionPreset= CLOCK_AND_LIGHT_BASED;
  } else {
    Serial.println("Transition not specified or set to manual");  
    transitionPreset= MANUAL;
  }
  
   if (root["brightness"] > MAX_BRIGHTNESS_ALLOWED) {
      Serial.println("Brightness set too high, limiting");
      targetBrightness = MAX_BRIGHTNESS_ALLOWED;
    } else {
      targetBrightness = root["brightness"];
    }

  // if not specified, root brightness is 0
  Serial.println("No brightness specified or 0");
  if (stateOn)
    targetBrightness = MAX_BRIGHTNESS_ALLOWED;
  else
    targetBrightness = 0;
  
  if (targetBrightness > currentBrightness) {
    startFadeOn = true;
    startFadeOff = false;
  } else if (targetBrightness < currentBrightness) {
    startFadeOn = false;
    startFadeOff = true;
  } else {
    startFadeOn = false;
    startFadeOff = false;
  }
  return true;
}


/********************************** START SEND STATE*****************************************/
void sendState(void) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["state"] = (stateOn) ? on_cmd : off_cmd;
  root["targetBrightness"] = targetBrightness;
  root["currentBrightness"] = currentBrightness;
  root["fadingon"] = startFadeOn ? true : false;
  root["fadingoff"] = startFadeOff ? true : false;
  root["transition][timeMs"] = transitionTimeMs;
  root["transition][preset"] = transitionPreset;
  
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish(light_state_topic, buffer, true);
}



/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(light_set_topic);
      sendState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


/********************************** LED brightness setter**********************************/
void setStripBrightness(int currentBrightness)
{
  if (currentBrightness > MAX_BRIGHTNESS_ALLOWED)
    currentBrightness = MAX_BRIGHTNESS_ALLOWED;

  if (currentBrightness > MAX_PWM_COMMAND)
    currentBrightness = MAX_PWM_COMMAND;

  if (currentBrightness <PWM_COMMANDED_ZERO_UNDER_COMMAND)
    currentBrightness = 0;

  analogWrite(CORRIDOR_LAMP,currentBrightness);
}

/********************************** START MAIN LOOP*****************************************/
void loop() {
    if (specialSerialMode) {
      if (Serial.available()) {
        while(Serial.available()) {
          if (serialBufferPos < (SERIAL_BUFFER_SIZE -1)) {
              serialBuffer[serialBufferPos] = Serial.read();
              if (serialBuffer[serialBufferPos] == '+'){
                specialBrightness+=50;
                if (specialBrightness > MAX_PWM_COMMAND)
                  specialBrightness = MAX_PWM_COMMAND;
                setStripBrightness(specialBrightness);
              } else if (serialBuffer[serialBufferPos] == '-'){
                specialBrightness-=50;
                if (specialBrightness < PWM_COMMANDED_ZERO_UNDER_COMMAND)
                  specialBrightness = 0 ;
                setStripBrightness(specialBrightness);
              }                
              serialBufferPos = 0;
          }
        }
      }
    } else if (serialMode) {
    if (Serial.available()) {
      if (currentBrightness == targetBrightness) {
        while(Serial.available()) {
          if (serialBufferPos < (SERIAL_BUFFER_SIZE -1)) {
              serialBuffer[serialBufferPos] = Serial.read();
              serialBufferPos++;
            if ( serialBuffer[serialBufferPos - 1] == '}') { 
              lightCommandCallback(light_set_topic,serialBuffer,serialBufferPos);
              serialBufferPos = 0;
            }
          } else {
            Serial.read();
            serialBufferPos = 0;
            break;
          }
        }
      } else {
        Serial.read();
      }
    }
  }
  else {
    if (!client.connected()) {
      reconnect();
    }

    if (WiFi.status() != WL_CONNECTED) {
      delay(1);
      Serial.print("WIFI Disconnected. Attempting reconnection.");
      setup_wifi();
      return;
    }

    client.loop();

    ArduinoOTA.handle();
  }
  
  if (!specialSerialMode) {
   if(currentBrightness != targetBrightness) {
     if (startFadeOn || startFadeOff) {
      Serial.print("first step fading from ");
      Serial.print(currentBrightness,DEC);
      Serial.print(" to ");
      Serial.print(targetBrightness,DEC);
      Serial.print(" in ");
      Serial.print(transitionTimeMs,DEC);
      Serial.println(" ms");

      // we want the max stepCount we can, so it depends wether transition time 
      // or brightness count is higher. 
      // we might want to set a minimum delay or minimum brightness step
      // so account for that
      
      if ((abs(targetBrightness - currentBrightness) / MIN_STEP_BRIGHTNESS) > (transitionTimeMs / MIN_STEP_DELAY)) {
      //we can do more than 1 brightness step per delay step ms. set stepDelay to that time
        stepDelay = MIN_STEP_DELAY;
        stepCount = transitionTimeMs / stepDelay;
        stepBrightness = (targetBrightness - currentBrightness) / stepCount;
      } else {
      //we cannot. brightness difference is the limiting factor.
        stepCount = abs(targetBrightness - currentBrightness) / MIN_STEP_BRIGHTNESS;
        stepDelay = transitionTimeMs / stepCount ;
        stepBrightness = (targetBrightness > currentBrightness)?MIN_STEP_BRIGHTNESS:-MIN_STEP_BRIGHTNESS;
      }

      if (currentBrightness < PWM_COMMANDED_ZERO_UNDER_COMMAND)
        currentBrightness = PWM_COMMANDED_ZERO_UNDER_COMMAND; 
      
      startFadeOn = false;
      startFadeOff = false;
      Serial.print("delay step ");
      Serial.print(stepDelay,DEC);
      Serial.print(", step count ");
      Serial.print(stepCount,DEC);
      Serial.print(", brightness step ");
      Serial.println(stepBrightness,DEC);
     }

    delay(stepDelay);
    currentBrightness += stepBrightness;
      
    setStripBrightness(currentBrightness);
    stepCount--;

    if (stepCount<=0) {
      Serial.println("Stop Fading");
      stepCount = 0;
      currentBrightness = targetBrightness;
    }
  }
 }
}
