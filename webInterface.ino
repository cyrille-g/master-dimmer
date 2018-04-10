uint8_t getIndexFromRoomName(const char *room) {
  uint8_t index = 0;
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
    if (strcmp (LUT_IndexToRoomName[index], room) == 0) {
      break;
    }
  }
  return index;
}


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
