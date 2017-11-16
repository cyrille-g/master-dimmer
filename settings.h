#ifndef SETTINGS_H
#define SETTINGS_H
/* DEFINES */


/**************************** OTA **************************************************/
#define SENSORNAME "sensorname" //change this to whatever you want to call your device
#define OTAPASSWORD "OTApwd" //the gConfPassword you will need to enter to upload remotely via the ArduinoIDE
#define OTAPORT 880

/******************************* MQTT **********************************************/
#define MQTT_MAX_PACKET_SIZE 512

/******************************** serial ********************************************/
#define SERIAL_BUFFER_SIZE 200

/***************************** fade *************************************************/
#define MIN_TRANSITION_TIME 500
#define MAX_TRANSITION_TIME 30000
#define MIN_STEP_DELAY 10
#define MIN_STEP_BRIGHTNESS 1

#define MANUAL 0
#define CLOCK_BASED 1
#define LIGHT_BASED 2
#define CLOCK_AND_LIGHT_BASED 3

/******************************** HARDWARE *******************************************/

#define CORRIDOR_LAMP D7
#define TOILET_LAMP D6
#define BEDROOM_LAMP D8
#define LED_CARTE BUILTIN_LED

#define MAX_BRIGHTNESS_ALLOWED 1000
#define MAX_PWM_COMMAND 1020
#define PWM_COMMANDED_ZERO_UNDER_COMMAND 5

/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) **************/
const char* gConfSsid = "ssid"; //type your WIFI information inside the quotes
const char* gConfPassword = "wifipwd";
const char* gConfMqttServerIp = "192.168.200.200";
const char* gConfMqttUsername = "mqttUsername";
const char* gConfMqttPassword = "mqttPassword";
const uint8_t gConfMqttPort = 1883;

/************* MQTT TOPICS (change these topics as you wish)  **********************/
const char* gConfLightStateTopic = "light/topic/state";
const char* gConfLightSetTopic = "light/topic/POWER";

const char* gConfOnCommand = "ON";
const char* gConfOffCommand = "OFF";

/**************************************** NETWORK **********************************/

// the media access control (ethernet hardware) address for the shield:
const uint8_t gConfMacAddress[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
const uint8_t gConfIpAddress[] = { 192, 168, 4, 166 };   


/**************************************** TIME **************************************/
#define GCONF_NTP_PACKET_SIZE 48   // NTP time stamp is in the first 48 bytes of the message

uint16_t gConfNtpServerPort = 4567; 
char gConftimeServer[] = "fr.pool.ntp.org";
uint8_t gConfNtpPacketBuffer[GCONF_NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

#endif
