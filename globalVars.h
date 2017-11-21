#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>



/******************************** structures**************************/
typedef struct transitionQueueElem_s {
  int16_t  stepBrightness;
  uint16_t stepDelay;
  uint16_t stepCount;
  STAILQ_ENTRY(transitionQueueElem_s) transitionEntry;
} transitionQueueElem_t;

transitionQueueElem_t *gpCurrentTransition[MAX_ROOM_COUNT] = {NULL};



/******************************** queue system ***********************/
typedef STAILQ_HEAD(, transitionQueueElem_s) transitionQueueHead_t;


/******************************** hardware ***************************/
int gOnboardLedState = HIGH;
bool gPowerNeeded = false;
/******************************** network ****************************/
WiFiClient   gEspClient;
PubSubClient gMqttClient(gEspClient);
ESP8266WebServer gServer(WEBSERVER_PORT);
String gWebPage = "";


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

/**************************** NTP management **************************/

#endif
