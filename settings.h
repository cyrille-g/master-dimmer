
/* DEFINES */


/**************************** OTA **************************************************/
#define SENSORNAME "sensorname" //change this to whatever you want to call your device
#define OTAPASSWORD "OTApwd" //the password you will need to enter to upload remotely via the ArduinoIDE
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


/* CONST VARS */
/**************************************** JSON *************************************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);


/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) **************/
const char* ssid = "ssid-8C8230B0"; //type your WIFI information inside the quotes
const char* password = "wifipwd";
const char* mqtt_server = "192.168.200.200";
const char* mqtt_username = "mqttUsername";
const char* mqtt_password = "mqttPassword";
const int mqtt_port = 1883;

/************* MQTT TOPICS (change these topics as you wish)  **********************/
const char* light_state_topic = "light/topic/state";
const char* light_set_topic = "light/topic/POWER";

const char* on_cmd = "ON";
const char* off_cmd = "OFF";

/**************************************** NETWORK **********************************/

// the media access control (ethernet hardware) address for the shield:
const byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
const byte ip[] = { 192, 168, 4, 166 };   

/* VARS */ 
/**************************************** NETWORK **********************************/

WiFiClient espClient;
PubSubClient client(espClient);


/******************************** serial ***********************************************/
bool serialMode = false;
byte serialBuffer[SERIAL_BUFFER_SIZE];
byte serialBufferPos = 0;
bool specialSerialMode = false;
int specialBrightness = 0;



/******************************** fade *************************************************/

int targetBrightness = 0;
int currentBrightness = 0;
int stepCount = MIN_STEP_COUNT;
int stepDelay = MIN_STEP_DELAY;
int stepBrightness = 0;

bool stateOn = false;
bool startFadeOn = false;
bool startFadeOff = false;

int transitionTimeMs = MIN_TRANSITION_TIME;
int transitionPreset = MANUAL;
int gOnboardLedState = HIGH;
