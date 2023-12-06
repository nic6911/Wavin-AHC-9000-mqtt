# Wavin-AHC-9000-mqtt
This is a simple Esp8266 mqtt interface for Wavin AHC-9000/Jablotron AC-116, with the goal of being able to control this heating controller from a home automation system. The fork here is just an edit to fit a ESP-01 for which I have made a PCB giving a very compact unit.

## Disclaimer
Do this at your own risk ! You are interfacing with hardware that you can potentially damage if you do not connect things as required !
Using the hardware and code presented here is done at you own risk. The hardware and software has been tested on Wavin AHC9000 and Nilan Comfort 300 without issues.

## Hardware

## ESP32C3
See here: https://github.com/nic6911/ESP32_Modbus_Module

## ESP8266
The Hardware used here is a design done by me and is a mutli-purpose ESP-01 Modbus module that was intended for Wavin AHC9000 and Nilan ventilation. But since it is pretty generic it will suit most modus applications.
The hardware includes buck converter supplying the ESP-01 and Modbus module with 3.3V from anything going from 8-24V (28V absolute max rating) as 12V and 24V are usually available on these systems for powering something like this.

### ESP8266 Revision 1.0
The AHC-9000 uses modbus to communicate over a half duplex RS422 connection. It has two RJ45 connectors for this purpose, which can both be used. 
The following schematic shows how my board is constructed in rev 1.0
![Schematic](/electronics/schematic.png)


My board design is made to fit with an ESP-01 board as seen here:
![Bottom](/electronics/Bottom.PNG)
![Top](/electronics/Top.PNG)


### ESP8266 Revision 2.1
To facilitate code versions using Modbus converters without the data direction controlled from the ESP I have implemented Automatic Direction Control. This also makes one more IO available for other uses.
I have decided to add 2 x Optocoupler, one on each available IO, to have isolated outputs which I intend to use for my Nilan system.
This effectively means that the rev 2.1 is a more general purpose hardware platform that in my case will be used for both my Wavin and Nilan setups.

The following schematic shows how my board is constructed in rev 2.1
![Schematic](/electronics/Rev2_1/schematic.PNG)

My board design rev 2.1 is seen here:
![Bottom](/electronics/Rev2_1/Bottom.PNG)
![Top](/electronics/Rev2_1/Top.PNG)

A wiring example on a Comfort 300 and Wavin AHC9000 is shown here:
![Top](/electronics/Rev2_1/Connections.png)

On the Wavin you simply use a patch cable (straight) and connect it from the module to the Modbus port and then you are done :)

### Common

For this setup to work you need:
My ESP-01 or ESP32C3 Modbus Interface board or similar
A programmer for the ESP-01 in case you use that

I use the widely available FTDI interface suited for the ESP-01 which requires a minor modification to enable programming mode. To enable programming of the board you need to short two pins for going into programming mode. I solved this with a pin-row and a jumper for selecting programming or not.
Look in the electronics folder for pictures of the programmer and the modification.

## Software

### ESP32C3
Supporting the ESP32C3 version of the modbus module using MQTT through Arduino

### ESP8266
Supporting the deprecated ESP8266 version of the board

### Video Tutorials
I have made a couple of video tutorials.

For setting up Home Assistant for MQTT, finding the Wavin client and adding zones looke here:
https://youtu.be/kwnt9SaQ6Jc

For the above to work you have to have a programmed ESP-01 talking modbus (like my module with ESP-01) which is shown next.

For programming the ESP-01 using a programmer look here:
https://youtu.be/PWJ3N4B8Pc4

If you have a pre-programmed ESP-01 with OTA support then you have to install the library dependencies as above but do not have to use a programmer. You can then program it like shown here:
https://youtu.be/2H5gkzoha98

### Important config changes

In your configuration.yaml you have to add:
```
mqtt:
  broker: IP address of the MQTT server (so you HA ip)
  username: user name selected previously
  password: password selected previously
  discovery: true
  discovery_prefix: homeassistant  
```
If you do not want automatic discovery of the zones of Wavin then read further down in the readme to see how.

The settings you just added in the MQTT setup is needed in the configuration of the Wavin ESP-01 software.
Go to the PrivateConfig.h and edit the settings:
```
WIFI_SSID = "Enter wireless SSID here";         // wifi ssid (name of your WiFi network)
WIFI_PASS = "Enter wireless password here";     // wifi password

MQTT_SERVER = "Enter mqtt server address here"; // mqtt server address without port number (your HA server address e.g. 192.168.0.1)
MQTT_USER   = "Enter mqtt username here";       // mqtt user. Use "" for no username (just created in the previous video tutorial)
MQTT_PASS   = "Enter mqtt password here";       // mqtt password. Use "" for no password (just created in the previous video tutorial)
MQTT_PORT   = 1883;                             // mqtt port
```


#### Arduino

Install the hardware support for ESP8266 as written in the readme under "Installing with Boards Manager"
https://github.com/esp8266/Arduino
Now, install the PubSubClient by Nick O'Leary through the Arduino Library Manager

#### Important !

When programmed insert the module into the board like shown in the picture and connect it to you AHC9000 using a regular ethernet cable.
IMPORTANT: To ensure a stable running of the switch mode supply, insert the ESP-01 prior to powering the board with the RJ45. A load on the switch mode on board is good for its stability.


### Testing
Assuming you have a working mqtt server setup, you should now be able to control your AHC-9000 using mqtt. If you have the [Mosquitto](https://mosquitto.org/) mqtt tools installed on your mqtt server, you can execude:
```
mosquitto_sub -u username -P password -t heat/# -v
```
to see all live updated parameters from the controller.

To change the target temperature for a output, use:
```
mosquitto_pub -u username -P password -t heat/floorXXXXXXXXXXXX/1/target_set -m 20.5
```
where the number 1 in the above command is the output you want to control and 20.5 is the target temperature in degree celcius. XXXXXXXXXXXX is the MAC address of the Esp8266, so it will be unique for your setup.

### Integration with HomeAssistant
If you have a working mqtt setup in [HomeAssistant](https://home-assistant.io/), all you need to do in order to control your heating from HomeAssistant is to enable auto discovery for mqtt in your `configuration.yaml`.
```
mqtt:
  discovery: true
  discovery_prefix: homeassistant
```
You will then get a climate and a battery sensor device for each configured output on the controller.

If you don't like auto discovery, you can add the entries manually. Create an entry for each output you want to control. Replace the number 0 in the topics with the id of the output and XXXXXXXXXXXX with the MAC of the Esp8266 (can be determined with the mosquitto_sub command shown above)
```
climate wavinAhc9000:
  - platform: mqtt
    name: floor_kitchen
    current_temperature_topic: "heat/floorXXXXXXXXXXXX/0/current"
    temperature_command_topic: "heat/floorXXXXXXXXXXXX/0/target_set"
    temperature_state_topic: "heat/floorXXXXXXXXXXXX/0/target"
    mode_command_topic: "heat/floorXXXXXXXXXXXX/0/mode_set"
    mode_state_topic: "heat/floorXXXXXXXXXXXX/0/mode"
    modes:
      - "heat"
      - "off"
    availability_topic: "heat/floorXXXXXXXXXXXX/online"
    payload_available: "True"
    payload_not_available: "False"
    qos: 0

sensor wavinBattery:
  - platform: mqtt
    state_topic: "heat/floorXXXXXXXXXXXX/0/battery"
    availability_topic: "heat/floorXXXXXXXXXXXX/online"
    payload_available: "True"
    payload_not_available: "False"
    name: floor_kitchen_battery
    unit_of_measurement: "%"
    device_class: battery
    qos: 0
```
