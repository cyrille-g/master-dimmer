/* The code was first based on @corbanmailloux work ( https://github.com/corbanmailloux/esp-mqtt-rgb-led ), now only the wireless and OTA part remains.
  To use this code you will need the following dependancies:
  - Support for the ESP8266 boards.
        - You can add it to the board manager by going to File -> Preference and pasting http://arduino.esp8266.com/stable/package_esp8266com_index.json into the Additional Board Managers URL field.
        - Next, download the ESP8266 dependancies by going to Tools -> Board -> Board Manager and searching for ESP8266 and installing it.
  - You will also need to download the follow libraries by going to Sketch -> Include Libraries -> Manage Libraries
      - PubSubClient
      - ArduinoJSON
      - NTPClientLib which uses timelib.

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

