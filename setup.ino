void debugSetup(void) {
  STAILQ_INIT(&gLogQueue);
  gWeblogCount = 0;
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
