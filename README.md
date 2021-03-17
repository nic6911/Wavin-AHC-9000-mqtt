# Wavin-AHC-9000-mqtt
This is a simple Esp8266 mqtt interface for Wavin AHC-9000/Jablotron AC-116, with the goal of being able to control this heating controller from a home automation system. The fork here is just an edit to fit a ESP-01 for which I have made a PCB giving a very compact unit.

## Hardware
The AHC-9000 uses modbus to communicate over a half duplex RS422 connection. It has two RJ45 connectors for this purpose, which can both be used. 
The following schematic shows how to connect an Esp8266 to the AHC-9000:
![Schematic](/electronics/schematic.png)

## Software

### Configuration
src/PrivateConfig.h contains 5 constants, that should be changed to fit your own setup.

`WIFI_SSID`, `WIFI_PASS`, `MQTT_SERVER`, `MQTT_USER`, and `MQTT_PASS`.

### Compiling
I use [PlatformIO](https://platformio.org/) for compiling, uploading, and and maintaining dependencies for my code. If you install PlatformIO in a supported editor, building this project is quite simple. Just open the directory containing `platformio.ini` from this project, and click build/upload. If you use a different board than nodemcu, remember to change the `board` variable in `platformio.ini`.
You may be able to use the Arduino tools with the esp8266 additions for compiling, but a few changes may be needed, including downloading dependencies manually.

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
