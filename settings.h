#ifndef _CGE_SETTINGS_EMPTY_H
#define _CGE_SETTINGS_EMPTY_H

/* DEFINES */


/**************************** OTA **************************************************/
#define DEVICENAME "dimmerdriver" //change this to whatever you want to call your device
#define OTAPASSWORD "otapwd" //the gConfPassword you will need to enter to upload remotely via the ArduinoIDE
#define OTAPORT 8800

/******************************* MQTT **********************************************/
#define MQTT_MAX_PACKET_SIZE 512

/******************************** serial ********************************************/
#define SERIAL_BUFFER_SIZE 200
#define LOG_BUFFER_SIZE 200

/******************************** DEBUG ********************************************/
#define WEB_DEBUG false
#define SERIAL_DEBUG false

/***************************** fade *************************************************/
#define MAX_ROOM_COUNT 4
#define MIN_TRANSITION_TIME 500
#define MAX_TRANSITION_TIME 30000
#define MIN_STEP_DELAY 20
#define MIN_STEP_BRIGHTNESS 1

#define MANUAL 0
#define CLOCK_BASED 1
#define LIGHT_BASED 2
#define CLOCK_AND_LIGHT_BASED 3

/******************************** HARDWARE *******************************************/
#define LED_CARTE BUILTIN_LED

/* PWM maximum value is defined as PWMRANGE on esp8266*/
#define MAX_PWM_COMMAND PWMRANGE

/* we want 80% of the max light */
#define MAX_BRIGHTNESS_ALLOWED (0.80 * MAX_PWM_COMMAND)
#define PWM_COMMANDED_ZERO_UNDER_COMMAND 5

/* index to pin LUT */
uint8_t LUT_IndexToPin[MAX_ROOM_COUNT] = {D0,D5,D6, D2};


/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) **************/
const char* gConfSsid = "ssid"; //type your WIFI information inside the quotes
const char* gConfPassword = "pwd";
const char* gConfMqttServerIp = "192.168.0.2";
const char* gConfMqttUsername = "mqttuser";
const char* gConfMqttPassword = "mqttpwd";
const int   gConfMqttPort = 1883;

/************* MQTT TOPICS (change these topics as you wish)  **********************/
#define MQTT_PREFIX "light"
#define MQTT_STATE "state"
#define MQTT_SET "POWER"
const char* gConfOnCommand = "ON";
const char* gConfOffCommand = "OFF";

const char* LUT_IndexToRoomName[MAX_ROOM_COUNT] = {"room1",
                                                   "room2",
                                                   "room3",
                                                   "room4"};

/**************************************** NETWORK **********************************/

// the media access control (ethernet hardware) address for the shield:
const byte gConfMacAddress[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }; 

/**************************************** TIME **************************************/
const uint16_t gConfNtpServerPort = 123;
const char gConftimeServer[] = "pool.ntp.org";


/****************************** TIME BASED TRANSITION*********************************/

const uint8_t gConfAutoDimStartHour = 22 ; // "time to start dimming" hour. Dimming will start from now.
const uint8_t gConfHourOfInflexion = 4  ; // "pitch blackest time of your night" hour. The closer to this hour, the dimmer the light.
const uint8_t gConfAutoDimStopHour = 10 ; // "time to stop dimming" hour. Dimming will be back to normal at that time.
const uint16_t gConfMaxDimmedBrightness = 300;  // this brightness is achieved at gConfHourOfInflexion
const uint16_t gConfMaxDimmedTransitionTime = 8000;  // this time will be used at gConfHourOfInflexion

/********************************** WEBSERVER ****************************************/
#define WEBSERVER_PORT 80
#define WEBLOG_QUEUE_SIZE 30

const String gRootWebserverMsg =  
"Any pCommand sets debug mode ON. Dimmer will not process any wifi switch command in debug mode\n"
"stop and reset set debug mode OFF.\n\n"
"Command list:\n"
"/pwm1: set pwm command at 200 for pwm1\n"
"/pwm2: set pwm command at 200 for pwm2\n"
"/pwm3: set pwm command at 200 for pwm3\n"
"/pwm4: set pwm command at 200 for pwm4\n"
"/podon: enable power supply 12V out\n"
"/podoff: disable power supply 12V out and tc4469 power output (also cuts TC4469 power)\n"
"/pwmenable: enable tc4469 power output at the pin. Needs podon before \n"
"/pwmdisable: disable tc4469 power output\n"
"/stop: stop debug mode\n"
"/reset: call ESP.reset\n"
"/status: show each brightness value and MQTT set topic";


#endif 
