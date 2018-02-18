/* The code was first based on @corbanmailloux work ( https://github.com/corbanmailloux/esp-mqtt-rgb-led ), now only the wireless and OTA part remains.

  To use this code you will need the following dependancies:

  - Support for the ESP8266 boards.
        - You can add it to the board manager by going to File -> Preference and pasting http://arduino.esp8266.com/stable/package_esp8266com_index.json into the Additional Board Managers URL field.
        - Next, download the ESP8266 dependancies by going to Tools -> Board -> Board Manager and searching for ESP8266 and installing it.

  - You will also need to download the follow libraries by going to Sketch -> Include Libraries -> Manage Libraries
      - PubSubClient
      - ArduinoJSON
      - NTPClientLib which uses time.lib.
      
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

uint8_t getIndexFromRoomName(const char *room) {
  uint8_t index = 0;
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
    if (strcmp (LUT_IndexToRoomName[index], room) == 0) {
      break;
    }
  }
  return index;
}

/********************************** PIN SETUP   ****************************************/
void pinSetup() {
  // set all pins to output mode and state except D1
  pinMode(D0, OUTPUT);        // pwm 1
  analogWrite(D0, 0);
  pinMode(D1, OUTPUT);        // enable pwm out.
  digitalWrite(D1, HIGH);     // this is connected to a NOT AND pin, so set it high at startup
  pinMode(D2, OUTPUT);        // pwm 4
  analogWrite(D2, 0);
  pinMode(D3, OUTPUT);        // power on demand
  digitalWrite(D3, LOW);      // set it to NO POWER on startup
  pinMode(D4, OUTPUT);        // not used
  digitalWrite(D4, LOW);
  pinMode(D5, OUTPUT);        // pwm 2
  analogWrite(D5, 0);
  pinMode(D6, OUTPUT);        // pwm 3
  analogWrite(D6, 0);
  pinMode(D7, OUTPUT);        // not used
  digitalWrite(D7, LOW);
  pinMode(D8, OUTPUT);        // not used
  digitalWrite(D8, LOW);

  // onboard led
  pinMode(LED_CARTE, OUTPUT);
}


void scenarioQueueSetup(void) {

  // Init scenario queues
  int index = 0;
  size_t topicSize = 0;
  Serial.println("MQTT set topics");
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
    STAILQ_INIT(&(gLedStrip[index].transitionQueue));
    /* init MQTT set and state topic: get the size first. do not forget trailing \0 and the 2 slashes */
    topicSize = strlen (MQTT_PREFIX) + strlen (LUT_IndexToRoomName[index]) + strlen (MQTT_STATE) + 3;
    gLedStrip[index].stateTopic = (char *)malloc(topicSize);
    sprintf(gLedStrip[index].stateTopic, "%s/%s/%s", MQTT_PREFIX, LUT_IndexToRoomName[index], MQTT_STATE);

    topicSize = strlen (MQTT_PREFIX) + strlen (LUT_IndexToRoomName[index]) + strlen (MQTT_SET) + 3;
    gLedStrip[index].setTopic = (char *)malloc(topicSize);
    sprintf(gLedStrip[index].setTopic, "%s/%s/%s", MQTT_PREFIX, LUT_IndexToRoomName[index], MQTT_SET);
#ifdef SERIAL_DEBUG
    Serial.println(gLedStrip[index].setTopic);
#endif
  }
}

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

#ifdef WEB_DEBUG
/* -------------------------------- DEBUG FUNCTIONS ---------------------------*/
void addLog(char *pLog) {
  while (gWeblogCount >= WEBLOG_QUEUE_SIZE) {
    weblogQueueElem_t *pOldLogelem = STAILQ_FIRST(&gLogQueue);
    STAILQ_REMOVE_HEAD(&gLogQueue, logEntry);
    free(pOldLogelem->pLogLine);
    free(pOldLogelem);
    gWeblogCount--;
  }
  weblogQueueElem_t *pNewLogElem = (weblogQueueElem_t *) malloc (sizeof (weblogQueueElem_t));
  char *pLogLine = (char *) malloc (strlen(pLog) + 1);
  strcpy(pLogLine, pLog);
  pNewLogElem->pLogLine = pLogLine;
  STAILQ_INSERT_TAIL(&gLogQueue, pNewLogElem, logEntry);
  gWeblogCount++;
}

void extractLog(String *pOutput) {
  pOutput->concat("\n\nLogs:\n ");
  weblogQueueElem_t *pLogElem = NULL;
  STAILQ_FOREACH(pLogElem, &gLogQueue, logEntry) {
     pOutput->concat(pLogElem->pLogLine);
     pOutput->concat("\n ");
  }
}

void debugSetup(void) {
  STAILQ_INIT(&gLogQueue);
  gWeblogCount = 0;
}
#endif
/* -------------------------------- WEBSERVER HANDLERS ------------------------*/

void handleRoot(void) {
  if (gDebugMode) {
    gWebServer.send(200, "text/plain", "Debug state: ON\n" + gRootWebserverMsg );
  } else {
    gWebServer.send(200, "text/plain", "Debug state: OFF\n" + gRootWebserverMsg );
  }
}


void handlePwm(void) {
  if (gWebServer.args() != 1) {
    handleStop();
    return;
  }
  gDebugMode = true;
  String roomToCommand(gWebServer.argName(0));
  const char* pwmName = roomToCommand.c_str();
  uint8_t pwmIndex = getIndexFromRoomName(pwmName);

  if (pwmIndex >= MAX_ROOM_COUNT) {
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"Cannot find room %s", pwmName);
  addLog(gLogBuffer);
  gWebServer.send(200, "text/plain", gLogBuffer);
#endif
  return;
  }
  uint16_t pwmValue = gWebServer.arg(0).toInt();
  if (pwmValue > MAX_PWM_COMMAND) {
    pwmValue = MAX_PWM_COMMAND;
  }

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"External pwm setting for room name %s index %d = %d", pwmName,pwmIndex, pwmValue);
  addLog(gLogBuffer);
#endif

  analogWrite(LUT_IndexToPin[pwmIndex], pwmValue);
  String webServerAnswer("index ");
  webServerAnswer.concat(pwmIndex);
  webServerAnswer.concat(" room ");
  webServerAnswer.concat(LUT_IndexToRoomName[pwmIndex]);
  webServerAnswer.concat(" commanded to ");
  webServerAnswer.concat(pwmValue);
  gWebServer.send(200, "text/plain", webServerAnswer.c_str());
}

void handlePwm1(void) {
  gDebugMode = true;
  analogWrite(LUT_IndexToPin[0], 200);
  gWebServer.send(200, "text/plain", "pwm1 commanded to 200");
}

void handlePwm2(void) {
  gDebugMode = true;
  analogWrite(LUT_IndexToPin[1], 200);
  gWebServer.send(200, "text/plain", "pwm2 commanded to 200");
}
void handlePwm3(void) {
  gDebugMode = true;
  analogWrite(LUT_IndexToPin[2], 200);
  gWebServer.send(200, "text/plain", "pwm3 commanded to 200");

}

void handlePwm4(void) {
  gDebugMode = true;
  analogWrite(LUT_IndexToPin[3], 200);
  gWebServer.send(200, "text/plain", "pwm4 commanded to 200");
}

void handleStatus(void) {
  gStatus = "Status:";
  int index = 0;
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
    gStatus.concat("\n\nRoom name: ");
    gStatus.concat(LUT_IndexToRoomName[index]);
    gStatus.concat("\n Available topics:\n  ");
    gStatus.concat(gLedStrip[index].setTopic);
    gStatus.concat("\n  ");
    gStatus.concat(gLedStrip[index].stateTopic);
    gStatus.concat("\n Current brightness:  ");
    gStatus.concat(gLedStrip[index].currentBrightness);
  }
  
  gStatus.concat("\n\nGlobals:\n gDebugMode: ");
  gStatus.concat(gDebugMode ? "true" : "false");
  gStatus.concat("\n gPowerNeeded: ");
  gStatus.concat(gPowerNeeded ? "true" : "false");
  gStatus.concat("\n gPrevPowerNeeded: ");
  gStatus.concat(gPrevPowerNeeded ? "true" : "false");
  gStatus.concat("\n\n Second to last command:\n");
  gStatus += gSecondLastCommand;
  gStatus.concat("\n Last command:\n");
  gStatus += gLastCommand;

  gStatus.concat("\nPin state:\n TC44 enable (inverted): ");
  gStatus.concat(digitalRead(D1));
  gStatus.concat("\n Power on demand: ");
  gStatus.concat(digitalRead(D3));
#ifdef WEB_DEBUG
  extractLog(&gStatus);
#endif
  gWebServer.send(200, "text/plain", gStatus.c_str());
}

void handlePowerOn(void) {
  gDebugMode = true;
  digitalWrite(D3, HIGH );
  gWebServer.send(200, "text/plain", "Power on demand ON");
}

void handlePowerOff(void) {
  gDebugMode = true;
  /* first cut TC4469 poweroutput */
  digitalWrite(D1, HIGH );
  delay(100);

  /* then cut the power on demand */
  digitalWrite(D3, LOW );
  gWebServer.send(200, "text/plain", "Power on demand OFF");
}
void handleTc4469EnableOutput(void) {
  gDebugMode = true;
  digitalWrite(D1, LOW );
  gWebServer.send(200, "text/plain", "TC4469 enable output");
}

void handleTc4469DisableOutput(void) {
  gDebugMode = true;
  digitalWrite(D1, HIGH );
  gWebServer.send(200, "text/plain", "TC4469 disable output");
}

void handleStop(void) {
  uint8 index = 0;
  digitalWrite(D1, HIGH );
  digitalWrite(D3, LOW );
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
    analogWrite(LUT_IndexToPin[index], 0);
    gDebugMode = false;
  }
  gWebServer.send(200, "text/plain", "Power on demand cut, tc4469 output disabled, all pwm set to 0");
}

void handleReset (void) {
  gWebServer.send(200, "text/plain", "Resetting in 1s");
  delay(1000);
  ESP.reset();
}

void processSyncEvent (NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
#ifdef WEB_DEBUG
  addLog("Time Sync error: ");
#endif
        if (ntpEvent == noResponse) {
#ifdef WEB_DEBUG
  addLog("NTP server not reachable");
#endif
        } else if (ntpEvent == invalidAddress) {
#ifdef WEB_DEBUG
  addLog("Invalid NTP server address");
#endif
        }
    } else {
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"Setting time to  %s and interval to 10mins / 1day",NTP.getTimeDateString(NTP.getLastNTPSync()).c_str());
  addLog(gLogBuffer);
#endif
    // set the system to do a sync every 86400 seconds when succesful, so once a day
    // if not successful, try syncing every 600s, so every 10 mins
    NTP.setInterval(600, 86400);

    }
}

/********************************** START SETUP ****************************************/
void setup() {

  // remove command log init
  gSecondLastCommand = "";
  gLastCommand = "";

  // serial
  Serial.begin(115200);

  //debug
#ifdef WEB_DEBUG
  debugSetup();
#endif
  //wifi not starting fix
  WiFi.mode(WIFI_OFF);
  toggleOnboardLed();

  // setup pins
  pinSetup();

  // setup scenario queues
  scenarioQueueSetup();

  uint8_t index = 0;
  byte received = 0;
  Serial.println("Press S for serial mode, T for test mode");
  delay(500);
  for (index = 0; (index < 10) && (!gSerialMode); index++) {
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

    /******************************* NTP setup **************************************/
    // NTP and time setup.
    // this one sets the NTP server as gConftimeServer with gmt+1
    // and summer/winter mechanic (true)
    NTP.begin(gConftimeServer, 1, true);
    // set the interval to 20s at first. On success, change that value 
    NTP.setInterval(20);

    NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
        gNtpEvent = event;
        gSyncEventTriggered = true;
        });

    /*******************************  Webserver setup *********************************/
    gWebServer.on("/", handleRoot);
    gWebServer.on("/pwm",handlePwm);
    gWebServer.on("/pwm1", handlePwm1);
    gWebServer.on("/pwm2", handlePwm2);
    gWebServer.on("/pwm3", handlePwm3);
    gWebServer.on("/pwm4", handlePwm4);
    gWebServer.on("/podon", handlePowerOn);
    gWebServer.on("/podoff", handlePowerOff);
    gWebServer.on("/pwmenable", handleTc4469EnableOutput);
    gWebServer.on("/pwmdisable", handleTc4469DisableOutput);
    gWebServer.on("/stop", handleStop);
    gWebServer.on("/reset", handleReset);
    gWebServer.on("/status", handleStatus);
    gWebServer.begin();

/******************************* MQTT setup **********************************/
#ifdef SERIAL_DEBUG
    Serial.println("Wifi connected, reaching for MQTT server");
#endif
    gMqttClient.setServer(gConfMqttServerIp, gConfMqttPort);
    gMqttClient.setCallback(lightCommandCallback);

/******************************* OTA setup *********************************/

    ArduinoOTA.setPort(OTAPORT);
    ArduinoOTA.setHostname(DEVICENAME);

    // authentication by default
    ArduinoOTA.setPassword(OTAPASSWORD);

    ArduinoOTA.onStart([]() {
#ifdef SERIAL_DEBUG      
      Serial.println("Starting OTA");
#endif
      gOtaUpdating = true;
      cutPowerOutput();
    });
    
    ArduinoOTA.onEnd([]() {
#ifdef SERIAL_DEBUG
      Serial.println("\nEnd");
#endif      
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
#ifdef SERIAL_DEBUG
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
#endif
    });
    ArduinoOTA.onError([](ota_error_t error) {
#ifdef SERIAL_DEBUG
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
#endif
    });

    ArduinoOTA.begin();
#ifdef SERIAL_DEBUG    
    Serial.println("Ready");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
#endif
  }
}


/********************************** START SETUP WIFI*****************************************/
void setup_wifi(void) {

#ifdef SERIAL_DEBUG
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(gConfSsid);
#endif

  delay(10);
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(gConfSsid, gConfPassword);
  delay(40);
  wl_status_t retCon = WiFi.status() ;

  while (retCon != WL_CONNECTED) {
#ifdef SERIAL_DEBUG
    WiFi.printDiag(Serial);
    Serial.print("Wifi status: ");
    switch (retCon) {
      case  WL_CONNECTED: Serial.println(" WL_CONNECTED"); break;
      case  WL_NO_SSID_AVAIL: Serial.println(" WL_NO_SSID_AVAIL"); break;
      case  WL_CONNECT_FAILED: Serial.println(" WL_CONNECT_FAILED (maybe wrong pwd)"); break;
      case  WL_IDLE_STATUS: Serial.println(" WL_IDLE_STATUS"); break;
      default: Serial.println("WL_DISCONNECTED"); break;
    }
#endif
    delay(200);
    toggleOnboardLed();
    retCon = WiFi.status() ;
  }
#ifdef SERIAL_DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
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
  sendState(index);
}

/***************************** empty a scenario queue including the playing one ***************************************/
void emptyScenarioQueue(uint8_t index) {
 // free the data in the queue
  transitionQueueElem_t *pTransition = NULL;
  STAILQ_FOREACH(pTransition, &(gLedStrip[index].transitionQueue), transitionEntry) {
    STAILQ_REMOVE_HEAD(&(gLedStrip[index].transitionQueue), transitionEntry);
    free(pTransition);
  } 
}

/***************************** linear scenario builder *******************************************/
transitionQueueElem_t * buildLinearScenario (int16_t startingBrightness, int16_t targetBrightness, uint16_t transitionTimeMs) {


#ifdef WEB_DEBUG
   sprintf(gLogBuffer,"linearscenario %d %d %d",
            startingBrightness, targetBrightness,transitionTimeMs);
  addLog(gLogBuffer);
#endif

  // we want the max stepCount we can, so it depends wether transition time
  // or brightness count is higher.
  // we might want to set a minimum delay or minimum brightness step
  // so account for that
  transitionQueueElem_t *pTransition = (transitionQueueElem_t *)malloc(sizeof(transitionQueueElem_t));
    if (pTransition == NULL) {
#ifdef WEB_DEBUG
  addLog("Transition malloced NULL");      
#endif
      return NULL;
    }
  if ((abs(targetBrightness - startingBrightness) / MIN_STEP_BRIGHTNESS) > (transitionTimeMs / MIN_STEP_DELAY)) {
    //we can do more than 1 brightness step per delay step ms. set stepDelay to that time
    pTransition->stepDelay = MIN_STEP_DELAY;
    pTransition->stepCount = transitionTimeMs / pTransition->stepDelay;
    pTransition->stepBrightness = (targetBrightness - startingBrightness) / pTransition->stepCount;
  } else {
    //we cannot. brightness difference is the limiting factor.
    pTransition->stepCount = abs(targetBrightness - startingBrightness) / MIN_STEP_BRIGHTNESS;
    pTransition->stepDelay = transitionTimeMs / pTransition->stepCount ;
    pTransition->stepBrightness = (targetBrightness > startingBrightness) ? MIN_STEP_BRIGHTNESS : -MIN_STEP_BRIGHTNESS;

  }
  /* whichever, save target brightness in the transition */
  pTransition->targetBrightness = targetBrightness;

#ifdef WEB_DEBUG
   sprintf(gLogBuffer,"res %d %d %d %u",
            pTransition->stepDelay, pTransition->stepCount,pTransition->stepBrightness,pTransition->targetBrightness);
  addLog(gLogBuffer);
#endif
  return pTransition;
}

/***************************** decreasing luminosity scenario building ***************************************/
transitionQueueElem_t * buildDecreaseLuminosityScenario (int16_t startingBrightness, uint16_t timeSinceStart)
{
  #ifdef WEB_DEBUG
  sprintf(gLogBuffer,"decLumScenario start br %d tstart %u",
           startingBrightness, timeSinceStart);
  addLog(gLogBuffer);
  #endif

  float timeRatio = timeSinceStart / (gConfStopLoweringLightOffset * 60.0);
  int32_t tmpTargetBrightness = 0;
  uint32_t tmpTransitionTimeMs = 0;
  int16_t  targetBrightness = 0;
  uint16_t transitionTimeMs = 0;
  /* this is the increase light scenario. Since we are clock based, 
  we do not receive target brightness or anything. what we know is if the light was already on
  we need to turn it off and vice versa */
  if (startingBrightness == 0)
  { 
    tmpTargetBrightness = (1.0 - timeRatio) * (MAX_BRIGHTNESS_ALLOWED - gConfTargetLowestBrightness)  + gConfTargetLowestBrightness;
    tmpTransitionTimeMs =timeRatio * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = (int16_t)tmpTargetBrightness;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  } else {
    tmpTransitionTimeMs =(1.0 - timeRatio) * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = 0;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  }
  
  if (startingBrightness == tmpTargetBrightness) {
  #ifdef WEB_DEBUG
    addLog("brightnesses are the same, change nothing");
  #endif
      return NULL;    
    }

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"decLumScenario start br %d target br %d transition time %u",
           startingBrightness, targetBrightness, transitionTimeMs);
  addLog(gLogBuffer);
#endif
  return buildLinearScenario (startingBrightness, targetBrightness,  transitionTimeMs);}

/***************************** increasing luminosity scenario building ***************************************/
transitionQueueElem_t * buildIncreaseLuminosityScenario (int16_t startingBrightness, uint16_t timeSinceStart)
{
  #ifdef WEB_DEBUG
  sprintf(gLogBuffer,"incLumScenario start br %d tstart %u",
           startingBrightness, timeSinceStart);
  addLog(gLogBuffer);
  #endif

  float timeRatio = timeSinceStart / (gConfStopDimmingOffset * 60.0);
  int32_t tmpTargetBrightness = 0;
  uint32_t tmpTransitionTimeMs = 0;
  int16_t  targetBrightness = 0;
  uint16_t transitionTimeMs = 0;
  /* this is the increase light scenario. Since we are clock based, 
  we do not receive target brightness or anything. what we know is if the light was already on
  we need to turn it off and vice versa */
  if (startingBrightness == 0)
  { 
    tmpTargetBrightness = (timeRatio * (MAX_BRIGHTNESS_ALLOWED - gConfTargetLowestBrightness))  + gConfTargetLowestBrightness;
    tmpTransitionTimeMs =(1.0 - timeRatio) * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = (int16_t)tmpTargetBrightness;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  } else {
    tmpTransitionTimeMs =(1.0 - timeRatio) * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = 0;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  }
  
  if (startingBrightness == tmpTargetBrightness) {
  #ifdef WEB_DEBUG
    addLog("brightnesses are the same, change nothing");
  #endif
      return NULL;    
    }

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"incLumScenario start br %d target br %d transition time %u",
           startingBrightness, targetBrightness, transitionTimeMs);
  addLog(gLogBuffer);
#endif
  return buildLinearScenario (startingBrightness, targetBrightness,  transitionTimeMs);
}

/***************************** time interval detection ***************************************/

uint16_t MinutesSinceStartOfInterval(uint8_t currentHour, uint8_t currentMinute, uint8_t startHour, uint8_t intervalSize)
{
  uint16_t timeSinceStart = 0;  

  if (startHour + intervalSize > 23 )  {
    /* the interval includes 0 */
    if ((currentHour >= startHour) && (currentHour < 0)) {
      /* first part of the clock */
      timeSinceStart =  ((currentHour - startHour ) * 60) + currentMinute;
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d between %d and 0h. interval %d tStart %d",
      currentHour,currentMinute,startHour,intervalSize,timeSinceStart);
  addLog(gLogBuffer);
#endif
    } else if ((currentHour >=0) && (currentHour < (startHour + intervalSize - 24))) {
      /* second part of the clock */
      timeSinceStart =  ((24 + currentHour - startHour  ) * 60) + currentMinute;
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d between 0h and max time. interval %d tStart %d",
      currentHour,currentMinute,startHour,intervalSize,timeSinceStart);
  addLog(gLogBuffer);
#endif
    } else {
      /* not in the interval, do nothing */
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d not in the interval",currentHour,currentMinute);
  addLog(gLogBuffer);
#endif
    }
  } else if ((currentHour>=startHour) && (currentHour < (startHour + intervalSize))) {
  /* the interval does not include 0 */
  timeSinceStart =  ((currentHour - startHour ) * 60) + currentMinute;
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d is between %d and interval %d. tStart %d",
      currentHour,currentMinute,startHour,intervalSize,timeSinceStart);
  addLog(gLogBuffer);
#endif
  } else {
    /* out of interval, value is 0 so do nothing */
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d not in the interval",currentHour,currentMinute);
  addLog(gLogBuffer);
#endif
  }
  return timeSinceStart;
}

/****************** accessor *******************************************/
transitionQueueElem_t *getScenarioStart(uint8_t index, int16_t *startingBrightness)
{
  transitionQueueElem_t *pBaseState = NULL;
    if (!STAILQ_EMPTY(&(gLedStrip[index].transitionQueue))) {
    /* yes, load the last inserted as reference */
    // does not work right now, missing implementation of __offsetof in arduino core 3.2.0
    // workaround since i know the structure size
#define  MY_STAILQ_LAST(head, type)          \
    (STAILQ_EMPTY((head)) ?           \
     NULL :             \
     ((struct type *)(void *)       \
((char *)((head)->stqh_last) - 8)))
    
    pBaseState = MY_STAILQ_LAST(&(gLedStrip[index].transitionQueue),transitionQueueElem_s);
    
    *startingBrightness = pBaseState->targetBrightness;
  } else if (gpCurrentTransition[index] != NULL) {
      /* yes, use the current playing scenario target */
     pBaseState = gpCurrentTransition[index];
     *startingBrightness = pBaseState->targetBrightness;
  } else {
    /* nothing playing. Use current */
    *startingBrightness = gLedStrip[index].currentBrightness;
  }
  
  /* clear any remaining or playing transition */
  if (gpCurrentTransition[index] != NULL) {
    free (gpCurrentTransition[index]);
    gpCurrentTransition[index] = NULL;
  }
  emptyScenarioQueue(index);

}

/****************** scenario builder from JSON *******************************************/
void buildAndInsertScenario (int16_t targetBrightness, uint16_t transitionTimeMs, uint8_t transitionPreset, uint8_t index)
{

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"scenario: br from %d to %d tr time %d preset %d room index %d",
      gLedStrip[index].currentBrightness, targetBrightness, transitionTimeMs,transitionPreset, index);
  addLog(gLogBuffer);
#endif

#ifdef SERIAL_DEBUG
  Serial.print("Scenario fading from ");
  Serial.print(gLedStrip[index].currentBrightness, DEC);
  Serial.print(" to ");
  Serial.print(targetBrightness, DEC);
  Serial.print(" in ");
  Serial.print(transitionTimeMs, DEC);
  Serial.print(" ms with preset: ");
  Serial.println(transitionPreset, DEC);
#endif

  /* first find where to start. 
   *  If there is a transition in the queue use the last inserted;
   *  If there is none, check if there is a transition playing;
   *  If there is none either, use the current state
   */
  int16_t startingBrightness = 0;
  transitionQueueElem_t *pBaseState = getScenarioStart(index, &startingBrightness);

  /* check if there is a transition to do */
  if (targetBrightness == startingBrightness) {
  /* none ! So do nothing */
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"Target brightness = starting brightness, no scenario");
  addLog(gLogBuffer);
#endif
     return ;
  }
  transitionQueueElem_t *pTransition=NULL;
  
   if (transitionPreset == CLOCK_BASED) {
     /* this is the clock based mode */
      /* in clock based mode, the more it gets near a set hour,
        the dimmer the light,
        the longer the transition */
        
      uint8_t currentHour = hour();
      uint8_t currentMinute = minute();
  
  #ifdef WEB_DEBUG
    sprintf(gLogBuffer,"CLOCK_BASED: currentHour %d currentMinute %d",
        currentHour, currentMinute);
    addLog(gLogBuffer);
  #endif
      /* time sections are like this:
       *  gConfAutoDimStartHour | gConfStopLoweringLightOffset | gConfStartIncreasingLightOffset | gConfStopDimmingOffset
       *  basically, the light output will do this:  \_______/
       *  
       *  the starting point of dimming is the first variable
       *  the starting point of lowest light is first variable + the second
       *  the starting point of less dimming is first variable + the second + the third
       *  and we go back to standard dimming when time reached the first variable + second + third + fourth
       *  
       *  working with offset is way simpler in terms of rollover.
       */
      uint16_t timeSinceInterval =  MinutesSinceStartOfInterval(currentHour, currentMinute, 
                                                                gConfAutoDimStartHour, gConfStopLoweringLightOffset);
  
      if (timeSinceInterval > 0 ) {
          pTransition = buildDecreaseLuminosityScenario(startingBrightness, timeSinceInterval); 
      } else {
        timeSinceInterval =  MinutesSinceStartOfInterval(currentHour, currentMinute, 
                                                        (gConfAutoDimStartHour + gConfStopLoweringLightOffset) % 24,
                                                        gConfStartIncreasingLightOffset);
        if (timeSinceInterval > 0 ) {
        /* this is the darkest light scenario.
        Buid the scenario with the lowest values */
          pTransition = buildLinearScenario(startingBrightness, gConfTargetLowestBrightness, gConfTargetLongestTransitionTime);           
        } else {
          timeSinceInterval =  MinutesSinceStartOfInterval(currentHour, currentMinute, 
                                                          (gConfAutoDimStartHour + gConfStopLoweringLightOffset + gConfStopLoweringLightOffset) % 24,
                                                          gConfStopDimmingOffset);
          if (timeSinceInterval > 0)  {
            /* this is the increasing light scenario */
#ifdef WEB_DEBUG
    sprintf(gLogBuffer,"buildIncreaseLuminosityScenario: timeSinceInterval %d",
        timeSinceInterval);
    addLog(gLogBuffer);
#endif             
             pTransition = buildIncreaseLuminosityScenario(startingBrightness, timeSinceInterval);
          } else  {
          /* nothing special is happening. This is "no dimming" time, build linear scenario */
          pTransition = buildLinearScenario(startingBrightness, targetBrightness, transitionTimeMs);
        }
      }
      }
// TODO : code these modes, if ever 
//   } else if (transitionPreset == LIGHT_BASED) {
//   /* this is the light based mode */
//      pTransition = buildLinearScenario(startingBrightness, targetBrightness, transitionTimeMs);
//   } else if (transitionPreset == CLOCK_AND_LIGHT_BASED) {
//   /*  this is the clock and light based mode */
//      pTransition = buildLinearScenario(startingBrightness, targetBrightness, transitionTimeMs);
   } else {
  /* this is the manual mode */
     pTransition = buildLinearScenario(startingBrightness,targetBrightness, transitionTimeMs);
  }
  
  if (pTransition!=NULL){
    STAILQ_INSERT_TAIL(&(gLedStrip[index].transitionQueue), pTransition, transitionEntry);
    #ifdef SERIAL_DEBUG
    Serial.print("Target brightness ");
    Serial.print(pTransition->targetBrightness, DEC);
    Serial.print("delay step ");
    Serial.print(pTransition->stepDelay, DEC);
    Serial.print(", step count ");
    Serial.print(pTransition->stepCount, DEC);
    Serial.print(", brightness step ");
    Serial.println(pTransition->stepBrightness, DEC);
    #endif
    #ifdef WEB_DEBUG
    sprintf(gLogBuffer,"Target brightness %d delay step %d, step count %d, brightness step %d",
      pTransition->targetBrightness, pTransition->stepDelay, pTransition->stepCount, pTransition->stepBrightness);
    addLog(gLogBuffer);
    #endif
  } else {
  #ifdef WEB_DEBUG
    addLog("Transition NULL");
  #endif
  }
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
  uint8_t  transitionPreset = MANUAL;

  /* The "state" command is self sufficient. If it is present,
     there is no need to parse anything else.
     Other data is relevant when state is not in the JSON . We still need to set
     targetBrightness, transitionTimeMs and transitionPreset */

  const char* res = root["state"];
  if (res) {
    if (strcmp(root["state"], gConfOnCommand) == 0 ) {
#ifdef SERIAL_DEBUG      
      Serial.println("State set ON");
#endif      
      targetBrightness = MAX_BRIGHTNESS_ALLOWED;
      transitionPreset = CLOCK_BASED;
    } else {
#ifdef SERIAL_DEBUG            
      Serial.println("State set and not ON, setting as OFF");
#endif
      transitionPreset = CLOCK_BASED;
      targetBrightness = 0;
    }
  } else { // if there is no STATE in the JSON
    res = root["transition"]["timeMs"];
    if (!res) {
#ifdef WEB_DEBUG
      addLog("No transition time set");
#endif
#ifdef SERIAL_DEBUG      
      Serial.println("No transition time set");
#endif
      transitionTimeMs = MIN_TRANSITION_TIME;
    } else {
      if (root["transition"]["timeMs"] <= MIN_TRANSITION_TIME) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition too short, setting to lower limit");
#endif        
#ifdef WEB_DEBUG        
        addLog("transition time too short");
#endif        
        transitionTimeMs = MIN_TRANSITION_TIME;
      } else if (root["transition"]["timeMs"] > MAX_TRANSITION_TIME) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition too short high, setting to higher limit");
#endif
#ifdef WEB_DEBUG        
          addLog("transition too long");
#endif
        transitionTimeMs = MAX_TRANSITION_TIME;
      
      } else {
#ifdef WEB_DEBUG   
        addLog("regular transition time found");
#endif        
        transitionTimeMs = root["transition"]["timeMs"];
      }
    }
    /* is there a preset ? */
    res = root["transition"]["preset"];
    if (res) {
      if (strcmp(root["transition"]["preset"], "CLOCK") == 0) {
        transitionPreset = CLOCK_BASED;
#ifdef SERIAL_DEBUG              
        Serial.println("Transition clock based");
#endif
      } else if (strcmp(root["transition"]["preset"], "LIGHT") == 0) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition light sensor based");
#endif        
        transitionPreset = LIGHT_BASED;
      } else if (strcmp(root["transition"]["preset"], "AUTO") == 0) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition both clock and light sensor based");
#endif        
        transitionPreset = CLOCK_AND_LIGHT_BASED;
      } else {
#ifdef SERIAL_DEBUG              
        Serial.println("Transition set to manual");
#endif     
        transitionPreset = MANUAL;
      }
    } else {
#ifdef SERIAL_DEBUG            
      Serial.println("Transition not specified");
#endif
      transitionPreset = MANUAL;
    }
  } // if there is transition in the JSON

  /* is there a brightness directive ? */
  res = root["brightness"];
  if (res) {
    if (root["brightness"] > MAX_BRIGHTNESS_ALLOWED) {
#ifdef SERIAL_DEBUG      
      Serial.println("Brightness set too high, limiting");
#endif
      targetBrightness = MAX_BRIGHTNESS_ALLOWED;
    } else {
      targetBrightness = root["brightness"];
    }
  } else { // if there is no brightness, set it to 0
#ifdef SERIAL_DEBUG      
    Serial.println("No brightness specified, set to 0");
#endif
targetBrightness = 0;
  } // if there is no brightness in the json

  //build and add the scenario

//  if (!gclockswitch) {
    buildAndInsertScenario(targetBrightness, transitionTimeMs, CLOCK_BASED, index);
//    gclockswitch = true;
//  } else {
//    buildAndInsertScenario(targetBrightness, transitionTimeMs, transitionPreset, index);
//  }
return true;
}

/********************************** START SEND STATE*****************************************/
void sendState(uint8_t index) {
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
        sendState(index);
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

/********************************** START MAIN LOOP*****************************************/
void loop() {
  if (gSpecialSerialMode) {
    if (Serial.available()) {
      while (Serial.available()) {
        if (gSerialBufferPos < (SERIAL_BUFFER_SIZE - 1)) {
          gSerialBuffer[gSerialBufferPos] = Serial.read();
          if (gSerialBuffer[gSerialBufferPos] == '+') {
            gSpecialBrightness += 50;
            if (gSpecialBrightness > MAX_PWM_COMMAND)
              gSpecialBrightness = MAX_PWM_COMMAND;
            setLedStripBrightness(gSpecialBrightness, 0);
          } else if (gSerialBuffer[gSerialBufferPos] == '-') {
            gSpecialBrightness -= 50;
            if (gSpecialBrightness < PWM_COMMANDED_ZERO_UNDER_COMMAND)
              gSpecialBrightness = 0 ;
            setLedStripBrightness(gSpecialBrightness, 0);
          }
          gSerialBufferPos = 0;
        }
      }
    }
  } else if (gSerialMode) {
    if (Serial.available()) {
      while (Serial.available()) {
        if (gSerialBufferPos < (SERIAL_BUFFER_SIZE - 1)) {
          gSerialBuffer[gSerialBufferPos] = Serial.read();
          gSerialBufferPos++;
          if ( gSerialBuffer[gSerialBufferPos - 1] == '\n') {
            Serial.print("send to topic ");
            Serial.println(gLedStrip[1].setTopic);
            lightCommandCallback(gLedStrip[1].setTopic, gSerialBuffer, gSerialBufferPos);
            gSerialBufferPos = 0;
          }
        } else {
          Serial.read();
          gSerialBufferPos = 0;
          break;
        }
      }
    }
  }
  else {
    if (!gMqttClient.connected()) {
      reconnectMqtt();
    }

    if (WiFi.status() != WL_CONNECTED) {
      delay(1);
#ifdef SERIAL_DEBUG      
      Serial.print("WIFI Disconnected. Attempting reconnection.");
#endif      
      setup_wifi();
      return;
    }

    gMqttClient.loop();

    ArduinoOTA.handle();
    gWebServer.handleClient();
  }

  /* process time only if no OTA is going on */
  if ((!gOtaUpdating) && gSyncEventTriggered) {
      processSyncEvent (gNtpEvent);
      gSyncEventTriggered = false;
  }
  

  /* we do not want to process anything in debug or OTA mode */
  if (gDebugMode || gOtaUpdating) {
    delay(50);
    return;
  }


  // are we processing a transition ?
  uint8_t transitionIndex = 0;
  gPowerNeeded = false;
  for (transitionIndex = 0; transitionIndex < MAX_ROOM_COUNT; transitionIndex++) {
    if (gpCurrentTransition[transitionIndex] == NULL) {
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
    if (!(gpCurrentTransition[transitionIndex] == NULL)) {
      delay(gpCurrentTransition[transitionIndex]->stepDelay);
      gLedStrip[transitionIndex].currentBrightness += gpCurrentTransition[transitionIndex]->stepBrightness;
      setLedStripBrightness(gLedStrip[transitionIndex].currentBrightness, transitionIndex);
      // did we reach the end ?
      if (gpCurrentTransition[transitionIndex]->stepCount == 0) {
        // yes. Set the brightness as planned, because sometime rounding error happen
        gLedStrip[transitionIndex].currentBrightness = gpCurrentTransition[transitionIndex]->targetBrightness;
        setLedStripBrightness(gLedStrip[transitionIndex].currentBrightness, transitionIndex);                
#ifdef SERIAL_DEBUG
        Serial.println("Stop Fading");
#endif
        //Free the transition.
        free(gpCurrentTransition[transitionIndex]);
        gpCurrentTransition[transitionIndex] = NULL;
      } else {
        // no. Decrease step count
        gpCurrentTransition[transitionIndex]->stepCount--;
      }
    } else {
      // no transition available, do nothing.
    }
    //We want to check if we need to switch power ON or OFF (or do nothing)
    gPowerNeeded |= (gLedStrip[transitionIndex].currentBrightness > 0);
  }

  if (gPowerNeeded && !gPrevPowerNeeded) {
    setPowerOutput();
  } else if (!gPowerNeeded && gPrevPowerNeeded) {
    cutPowerOutput();
  }

  gPrevPowerNeeded = gPowerNeeded;
}

