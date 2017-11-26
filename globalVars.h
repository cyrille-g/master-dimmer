#ifndef _CGE_GLOBAL_VARS_H
#define _CGE_GLOBAL_VARS_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
 


/******************************** structures**************************/
typedef struct transitionQueueElem_s {
  int16_t  stepBrightness;
  uint16_t stepDelay;
  uint16_t stepCount;
  STAILQ_ENTRY(transitionQueueElem_s) transitionEntry;
} transitionQueueElem_t;

transitionQueueElem_t *gpCurrentTransition[MAX_ROOM_COUNT] = {NULL,NULL,NULL,NULL};

typedef struct weblogQueueElem_s {
  char *pLogLine;
  STAILQ_ENTRY(weblogQueueElem_s) logEntry;
} weblogQueueElem_t;

/******************************** queue system ***********************/
typedef STAILQ_HEAD(, transitionQueueElem_s) transitionQueueHead_t;
typedef STAILQ_HEAD(, weblogQueueElem_s) weblogQueueHead_t;

/******************************** hardware ***************************/
int gOnboardLedState = HIGH;
bool gPowerNeeded = false;
bool gPrevPowerNeeded = false;
bool gOtaUpdating = false;
/******************************** network ****************************/
WiFiClient   gEspClient;
PubSubClient gMqttClient(gEspClient);
ESP8266WebServer gWebServer(WEBSERVER_PORT);

/******************************* debug  ******************************/
bool    gDebugMode = false;
String  gStatus;
String  gLastCommand;
String  gSecondLastCommand;

weblogQueueHead_t gLogQueue;
uint8_t gWeblogCount;
char gLogBuffer[LOG_BUFFER_SIZE] = {0};

/******************************** serial *****************************/
bool    gSerialMode = false;
byte    gSerialBuffer[SERIAL_BUFFER_SIZE];
byte    gSerialBufferPos = 0;
bool    gSpecialSerialMode = false;
int16_t gSpecialBrightness = 0;

/* CONST VARS */
/**************************************** JSON ************************/
const int JSON_BUFFER_SIZE = JSON_OBJECT_SIZE(10);

/******************************light management ***********************/
typedef struct {
  int16_t currentBrightness = 0;
  char *setTopic = NULL;
  char *stateTopic = NULL;
  transitionQueueHead_t transitionQueue;
} roomLight_t;

roomLight_t gLedStrip[MAX_ROOM_COUNT];

#endif
