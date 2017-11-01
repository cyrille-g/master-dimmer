# master-dimmer

The ESP8266 sends PWMs to control led strips, either using wifi or serial commands. 

The code works (although no wifi, but i might have fried that because it should).

It currently only controls one led strip on the D7 pin, and applies the state, brightness, and transition time parameters specified in JSON format. 

Make sure to change the default values in settings.h .


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
- Apply coding rules
- Add light sensor code (sensor connected on the analog input)
- Add ntp clock usage
- Add pattern code 
- update JSON commands for other lamps


## Warning

Do NOT try to connect your led strip on D7, you need a mosfet that can work with 3.3V level, or any kind of mosfet driver that will take that voltage level.

I will publish the schematics when i have tested it enough. For now it seems to work pretty well and  does not heat, but be extra careful when assembling and trying it; remember this circuit can command tens of amps, which can easily start a fire. If your AC/DC converter is badly wired, it could also kill you, so if you do not know for sure what you are doing, please ask someone who does to do the assembling and testing for you.

Your milage may vary, i will not take any responsibility if you end up frying something.
