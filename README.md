# Wavin-AHC-9000-mqtt
This is a simple mqtt interface for Wavin AHC-9000/Jablotron AC-116, with the goal of being able to control this heating controller from a home automation system. 

## Homey App and install instructions

I have not done this myself as I run Home Assistant - but there's guides from user here:

Homey App from a user (Filip B Bech): https://homey.app/en-dk/app/com.wavin.ahc9000/Wavin-AHC-9000/

Guide from a user (Jan): https://github.com/nic6911/Wavin-AHC-9000-mqtt/blob/master/doc/Homey%20guide%20til%20Wavin%20AHC%209000%20ModBus%20installation.pdf

## Hardware

## ESP32C3
See here: https://github.com/nic6911/ESP32_Modbus_Module


## Software

### ESP32C3
Supporting the ESP32C3 version of the modbus module using MQTT through Arduino

### Video Tutorials
I have made a couple of video tutorials.

For setting up Home Assistant for MQTT, finding the Wavin client and adding zones looke here:
https://youtu.be/kwnt9SaQ6Jc

For the above to work you have to have a programmed ESP device talking modbus. I have made this video tutorial for ESP-01, but similar procedure is viable for the new ESP32-C3.
https://youtu.be/PWJ3N4B8Pc4

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

## Disclaimer
Do this at your own risk ! You are interfacing with hardware that you can potentially damage if you do not connect things as required !
Using the hardware and code presented here is done at you own risk. The hardware and software has been tested on Wavin AHC9000 and Nilan Comfort 300 without issues.