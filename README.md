# Wavin-AHC-9000-mqtt
This is a simple Esp8266 mqtt interface for Wavin AHC-9000/Jablotron AC-116, with the goal of being able to control this heating controller from a home automation system. The fork here is just an edit to fit a ESP-01 for which I have made a PCB giving a very compact unit.

## Hardware
The AHC-9000 uses modbus to communicate over a half duplex RS422 connection. It has two RJ45 connectors for this purpose, which can both be used. 
The following schematic shows how to connect an Esp8266 to the AHC-9000:
![Schematic](/electronics/schematic.png)


My board design is made to fit with an ESP-01 board as seen here:
![Bottom](/electronics/Bottom.png)
![Top](/electronics/Top.png)

For this setup to work you need:
My ESP-01 Modus Interface board or similar
An ESP-01
A programmer for the ESP-01

I use the widely available FTDI interface suited for the ESP-01 which requires a minor modification to enable programming mode. To enable programming of the board you need to short two pins for going into programming mode. I solved this with a pin-row and a jumper for selecting programming or not.
Look in the electronics folder for pictures of the programmer and the modification.

## Software

### My addition to the description:

You can do either Visual Sudio code OR Arduino - for some reason Visual studio code does not always work for ESP-01 so i prefer Arduino...

Now you have to change the few variables in the code first:
                           
Setup the MQTT broker in HA like shown in the video at these timeslots:
Time: 2minuts - 4 minuts and then again at 9:30 minutes to 10:15 minutes.
https://www.youtube.com/watch?v=1uxRvbbd0fc

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

#### Platformio (Visual Code Studio)
You have to install visual code studio (free) and the platformio extension https://platformio.org/ (google it if you have challenges).
You then have to download the software here on my git, extract it and open it through the platformio extension in visual code studio.
Now, you can hit program down in the left'ish corner of the visual code studio window (tooltips come when you hover the mouse over the small icons).


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
