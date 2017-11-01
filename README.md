# master-dimmer

The ESP8266 sends PWMs to control led strips, either using wifi or serial commands. 

It currently only controls one led strip on the D7 pin, and applies the state, brightness, and transition time parameters specified in JSON format. 

Json examples:

{ "state": "ON" }

{  "state": "ON" ,
   "brightness": 600}
  
{ "state": "ON"
  "brightness": 600,
  "transition" : { "preset": "AUTO" }}

{  "state": "ON"
  "transition" : { "preset": "CLOCK", "TimeMs": 2000 }}
  
{  "state": "ON"
  "brightness": 600,
  "transition" : { "preset": "CLOCK", "TimeMs": 2000 }}

Consider it only works in serial mode right now, for some reason it registers on my wifi network but still says it is disconnected.

This is my first github project and more an experiment than something else, so please be patient and show forgiveness :)

The code was first based on @corbanmailloux work (https://github.com/corbanmailloux/esp-mqtt-rgb-led), but i only kept the wifi part in the end.

TODO:
Apply coding rules
Add light sensor code (sensor connected on the analog input)
Add ntp clock usage
Add pattern code 
update JSON commands for other lamps

