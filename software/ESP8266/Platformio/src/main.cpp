#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "WavinController.h"
#include "PrivateConfig.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

char apSSID[] = "ESP01Modbus";
char apPass[] = "11111111";

const char PAGE_index[] PROGMEM = R"=====(
<!DOCTYPE html>
<html><head><title>Flash this ESP8266!</title></head><body>
<h2>Welcome!</h2>
You are successfully connected to your ESP8266 via its WiFi.<br>
Please click the button to proceed and upload a new binary firmware!<br><br>
<b>Be sure to double check the firmware (.bin) is suitable for your chip!<br>
I am not to be held liable if you accidentally flash a cat pic instead or something goes wrong during the update!<br>
You are solely responsible for using this tool!</b><br><br>
<form><input type="button" value="Select firmware..." onclick="window.location.href='/update'" />
</form><br>
(c) 2017 Christian Schwinne <br>
<i>Licensed under the MIT license</i> <br>
<i>Uses libraries:</i> <br>
<i>ESP8266 Arduino Core</i> <br>
</body></html>
)=====";

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

bool enableHttpUpdater = true;

// MQTT defines
// Esp8266 MAC will be added to the device name, to ensure unique topics
// Default is topics like 'heat/floorXXXXXXXXXXXX/3/target', where 3 is the output id and XXXXXXXXXXXX is the mac
const String   MQTT_PREFIX              = "heat/";       // include tailing '/' in prefix
const String   MQTT_DEVICE_NAME         = "floor";       // only alfanumeric and no '/'
const String   MQTT_ONLINE              = "/online";      
const String   MQTT_SUFFIX_CURRENT      = "/current";    // include heading '/' in all suffixes
const String   MQTT_SUFFIX_SETPOINT_GET = "/target";
const String   MQTT_SUFFIX_SETPOINT_SET = "/target_set";
const String   MQTT_SUFFIX_MODE_GET     = "/mode";
const String   MQTT_SUFFIX_MODE_SET     = "/mode_set";
const String   MQTT_SUFFIX_CURRENTFLOOR = "/floortemperature";
const String   MQTT_SUFFIX_HUMIDITY     = "/humidity";
const String   MQTT_SUFFIX_DEWPOINT     = "/dewpoint";
const String   MQTT_SUFFIX_BATTERY      = "/battery";
const String   MQTT_SUFFIX_OUTPUT       = "/output";

const String   MQTT_UPDATE              = "update";
const String   MQTT_VALUE_ENABLE        = "enable";

const String   MQTT_VALUE_MODE_STANDBY  = "off";
const String   MQTT_VALUE_MODE_MANUAL   = "heat";

const String   MQTT_CLIENT = "Wavin-AHC-9000-mqtt";       // mqtt client_id prefix. Will be suffixed with Esp8266 mac to make it unique

String mqttDeviceNameWithMac;
String mqttClientWithMac;

// Operating mode is controlled by the MQTT_SUFFIX_MODE_ topic.
// When mode is set to MQTT_VALUE_MODE_MANUAL, temperature is set to the value of MQTT_SUFFIX_SETPOINT_
// When mode is set to MQTT_VALUE_MODE_STANDBY, the following temperature will be used
const float STANDBY_TEMPERATURE_DEG = 5.0;

const uint8_t TX_ENABLE_PIN = 2;
const bool SWAP_SERIAL_PINS = false;
const uint16_t RECIEVE_TIMEOUT_MS = 1000;
WavinController wavinController(TX_ENABLE_PIN, SWAP_SERIAL_PINS, RECIEVE_TIMEOUT_MS);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastUpdateTime = 0;

const uint16_t POLL_TIME_MS = 5000;

struct lastKnownValue_t {
  uint16_t airtemperature;
  uint16_t humidity;
  uint16_t dewpoint;
  uint16_t floortemperature;
  uint16_t setpoint;
  uint16_t battery;
  uint16_t status;
  uint16_t mode;
} lastSentValues[WavinController::NUMBER_OF_CHANNELS];

const uint16_t LAST_VALUE_UNKNOWN = 0xFFFF;

bool configurationPublished[WavinController::NUMBER_OF_CHANNELS];


// Read a float value from a non zero terminated array of bytes and
// return 10 times the value as an integer
uint16_t temperatureFromString(String payload)
{
  float targetf = payload.toFloat();
  return (unsigned short)(targetf * 10);
}


// Returns temperature in degrees with one decimal
String temperatureAsFloatString(uint16_t temperature)
{
  float temperatureAsFloat = ((float)temperature) / 10;
  return String(temperatureAsFloat, 1);
}


uint8_t getIdFromTopic(char* topic)
{
  unsigned int startIndex = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/").length();
  int i = 0;
  uint8_t result = 0;

  while(topic[startIndex+i] != '/' && i<3)
  {
    result = result * 10 + (topic[startIndex+i]-'0');
    i++;
  }

  return result;
}


void resetLastSentValues(); // declare function as it is needed in mqttCallback

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String topicString = String(topic);
  
  char terminatedPayload[length+1];
  for(unsigned int i=0; i<length; i++)
  {
    terminatedPayload[i] = payload[i];
  }
  terminatedPayload[length] = 0;
  String payloadString = String(terminatedPayload);

  uint8_t id = getIdFromTopic(topic) - 1; // convert to zero-index. Topic correspond to 1 as first index

  if(topicString.endsWith(MQTT_SUFFIX_SETPOINT_SET))
  {
    uint16_t target = temperatureFromString(payloadString);
    wavinController.writeRegister(WavinController::CATEGORY_PACKED_DATA, id, WavinController::PACKED_DATA_MANUAL_TEMPERATURE, target);
  }
  else if(topicString.endsWith(MQTT_SUFFIX_MODE_SET))
  {
    if(payloadString == MQTT_VALUE_MODE_MANUAL) 
    {
      wavinController.writeMaskedRegister(
        WavinController::CATEGORY_PACKED_DATA,
        id,
        WavinController::PACKED_DATA_CONFIGURATION,
        WavinController::PACKED_DATA_CONFIGURATION_MODE_MANUAL,
        ~WavinController::PACKED_DATA_CONFIGURATION_MODE_MASK);
    }
    else if (payloadString == MQTT_VALUE_MODE_STANDBY)
    {
      wavinController.writeMaskedRegister(
        WavinController::CATEGORY_PACKED_DATA, 
        id, 
        WavinController::PACKED_DATA_CONFIGURATION, 
        WavinController::PACKED_DATA_CONFIGURATION_MODE_STANDBY, 
        ~WavinController::PACKED_DATA_CONFIGURATION_MODE_MASK);
    }
  }
  else if(topicString.endsWith(MQTT_UPDATE))
  {
    if(payloadString == MQTT_VALUE_ENABLE) 
    {
      enableHttpUpdater = true;
    }
  }
  else if(topicString.equalsIgnoreCase("homeassistant/status") || topicString.equalsIgnoreCase("hass/status"))
  {
    // resend values on mqtt when homeassistant gets online
    if(payloadString.equalsIgnoreCase("online")) 
    {
      resetLastSentValues();
    }
  }

  // Force re-read of registers from controller now
  lastUpdateTime = 0;

}


void resetLastSentValues()
{
  for(int8_t i=0; i<WavinController::NUMBER_OF_CHANNELS; i++)
  {
    lastSentValues[i].airtemperature = LAST_VALUE_UNKNOWN;
    lastSentValues[i].humidity = LAST_VALUE_UNKNOWN;
    lastSentValues[i].dewpoint = LAST_VALUE_UNKNOWN;
    lastSentValues[i].floortemperature = LAST_VALUE_UNKNOWN;
    lastSentValues[i].setpoint = LAST_VALUE_UNKNOWN;
    lastSentValues[i].battery = LAST_VALUE_UNKNOWN;
    lastSentValues[i].status = LAST_VALUE_UNKNOWN;
    lastSentValues[i].mode = LAST_VALUE_UNKNOWN;

    configurationPublished[i] = false;
  }
}


void publishIfNewValue(String topic, String payload, uint16_t newValue, uint16_t *lastSentValue)
{
  if (newValue != *lastSentValue)
  {
    if (mqttClient.publish(topic.c_str(), payload.c_str(), false))
    {
      *lastSentValue = newValue;
    }
    else
    {
      *lastSentValue = LAST_VALUE_UNKNOWN;
    }
  }
}


// Publish discovery messages for HomeAssistant
// See https://www.home-assistant.io/docs/mqtt/discovery/
void publishConfiguration(uint8_t channel)
{
  uint8_t announcedChannel = channel + 1; // as the channels on the ahc-9000 begins with 1 not 0
  String mqttFirstPart = MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel;

  String climateTopic = String("homeassistant/climate/" + mqttDeviceNameWithMac + "/" + announcedChannel + "/config");
  String climateMessage = String(
    "{\"name\": \"" + mqttDeviceNameWithMac + "_" + announcedChannel +  "_climate\", "
    "\"current_temperature_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_CURRENT + "\", " 
    "\"temperature_command_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_SETPOINT_SET + "\", " 
    "\"temperature_state_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_SETPOINT_GET + "\", " 
    "\"mode_command_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_MODE_SET + "\", " 
    "\"mode_state_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_MODE_GET + "\", " 
    "\"modes\": [\"" + MQTT_VALUE_MODE_MANUAL + "\", \"" + MQTT_VALUE_MODE_STANDBY + "\"], " 
    "\"availability_topic\": \"" + MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE +"\", "
    "\"payload_available\": \"True\", "
    "\"payload_not_available\": \"False\", "
    "\"qos\": \"0\"}"
  );
  mqttClient.publish(climateTopic.c_str(), climateMessage.c_str(), true);  
  
  
  String sensorsTopic = String("homeassistant/binary_sensor/" + mqttDeviceNameWithMac + "/" + announcedChannel + "_output/config");
  String sensorsMessage = String(
    "{\"name\": \"" + mqttDeviceNameWithMac + "_" + announcedChannel +  "_output\", "
    "\"state_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_OUTPUT + "\", " 
    "\"payload_on\": \"on\", "
    "\"payload_off\": \"off\", "
    "\"availability_topic\": \"" + MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE +"\", "
    "\"payload_available\": \"True\", "
    "\"payload_not_available\": \"False\", "
    "\"qos\": \"0\"}"
  );
  mqttClient.publish(sensorsTopic.c_str(), sensorsMessage.c_str(), true);

  sensorsTopic = String("homeassistant/sensor/" + mqttDeviceNameWithMac + "/" + announcedChannel + "_floortemp/config");
  sensorsMessage = String(
    "{\"name\": \"" + mqttDeviceNameWithMac + "_" + announcedChannel +  "_floortemperature\", "
    "\"state_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_CURRENTFLOOR + "\", " 
    "\"device_class\": \"temperature\", "
    "\"unit_of_measurement\": \"°C\", " 
    "\"availability_topic\": \"" + MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE +"\", "
    "\"payload_available\": \"True\", "
    "\"payload_not_available\": \"False\", "
    "\"qos\": \"0\"}"
  );
  mqttClient.publish(sensorsTopic.c_str(), sensorsMessage.c_str(), true);

  sensorsTopic = String("homeassistant/sensor/" + mqttDeviceNameWithMac + "/" + announcedChannel + "_humidity/config");
  sensorsMessage = String(
    "{\"name\": \"" + mqttDeviceNameWithMac + "_" + announcedChannel +  "_humidity\", "
    "\"state_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_HUMIDITY + "\", " 
    "\"device_class\": \"humidity\", "
    "\"unit_of_measurement\": \"%\", " 
    "\"availability_topic\": \"" + MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE +"\", "
    "\"payload_available\": \"True\", "
    "\"payload_not_available\": \"False\", "
    "\"qos\": \"0\"}"
  );
  mqttClient.publish(sensorsTopic.c_str(), sensorsMessage.c_str(), true);


  sensorsTopic = String("homeassistant/sensor/" + mqttDeviceNameWithMac + "/" + announcedChannel + "_battery/config");
  sensorsMessage = String(
    "{\"name\": \"" + mqttDeviceNameWithMac + "_" + announcedChannel +  "_battery\", "
    "\"state_topic\": \"" + mqttFirstPart + MQTT_SUFFIX_BATTERY + "\", " 
    "\"availability_topic\": \"" + MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE +"\", "
    "\"payload_available\": \"True\", "
    "\"payload_not_available\": \"False\", "
    "\"device_class\": \"battery\", "
    "\"unit_of_measurement\": \"%\", "
    "\"qos\": \"0\"}"
  );
  mqttClient.publish(sensorsTopic.c_str(), sensorsMessage.c_str(), true);

  
  configurationPublished[channel] = true;
}


void setup()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char macStr[13] = {0};
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  mqttDeviceNameWithMac = String(MQTT_DEVICE_NAME + macStr);
  mqttClientWithMac = String(MQTT_CLIENT + macStr);

  mqttClient.setServer(MQTT_SERVER.c_str(), MQTT_PORT);
  mqttClient.setCallback(mqttCallback);


  uint8_t fails = 0;
  //try to connect to WiFi for 3 times or launch webserver
  while(fails<2 && WiFi.status() != WL_CONNECTED)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
      if (WiFi.waitForConnectResult() != WL_CONNECTED)
      {
        fails++;
      }
    }
    delay(1000);
  }
  if(fails>1)
  {
    WiFi.mode(WIFI_OFF);
    WiFi.softAP(apSSID, apPass);
    server.onNotFound([](){
      server.send(200, "text/html", PAGE_index);
    });
    httpUpdater.setup(&server);
    server.begin();
  
    while(1)
    {
      server.handleClient();
    }
  }

  enableHttpUpdater = false;  // disable as succesfully connected
}


bool updateServerInitialised = false;

void loop()
{
  if(enableHttpUpdater) // allow update of the esp
  {
    //WiFi.mode(WIFI_OFF);
    //WiFi.softAP(apSSID, apPass);
    if (!updateServerInitialised) // initialise the httpUpdater
    {
    server.onNotFound([](){
      server.send(200, "text/html", PAGE_index);
    });
    httpUpdater.setup(&server);
    server.begin();

      updateServerInitialised = true;
    }
  
      server.handleClient();
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    if (WiFi.waitForConnectResult() != WL_CONNECTED) return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!mqttClient.connected())
    {
      String will = String(MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE);
      if (mqttClient.connect(mqttClientWithMac.c_str(), MQTT_USER.c_str(), MQTT_PASS.c_str(), will.c_str(), 1, true, "False") )
      {
          String setpointSetTopic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/+" + MQTT_SUFFIX_SETPOINT_SET);
          mqttClient.subscribe(setpointSetTopic.c_str(), 1);
          
          String modeSetTopic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/+" + MQTT_SUFFIX_MODE_SET);
          mqttClient.subscribe(modeSetTopic.c_str(), 1);

          String updateTopic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + MQTT_UPDATE);
          mqttClient.subscribe(updateTopic.c_str(), 1);
          
          mqttClient.subscribe("homeassistant/status", 1);
          mqttClient.subscribe("hass/status", 1);
          
          mqttClient.publish(will.c_str(), (const uint8_t *)"True", 4, true);

          // Forces resending of all parameters to server
          resetLastSentValues();
      }
      else
      {
          return;
      }
    }
  
    // Process incomming messages and maintain connection to the server
    if(!mqttClient.loop())
    {
        return;
    }

    if (lastUpdateTime + POLL_TIME_MS < millis())
    {
      lastUpdateTime = millis();

      uint16_t registers[11];

      for(uint8_t channel = 0; channel < WavinController::NUMBER_OF_CHANNELS; channel++)
      {
        uint8_t announcedChannel = channel + 1;
        if (wavinController.readRegisters(WavinController::CATEGORY_CHANNELS, channel, WavinController::CHANNELS_PRIMARY_ELEMENT, 1, registers))
        {
          uint16_t primaryElement = registers[0] & WavinController::CHANNELS_PRIMARY_ELEMENT_ELEMENT_MASK;
          bool allThermostatsLost = registers[0] & WavinController::CHANNELS_PRIMARY_ELEMENT_ALL_TP_LOST_MASK;

          if(primaryElement==0)
          {
              // Channel not used
              continue;
          }

          if(!configurationPublished[channel])
          {
            uint16_t standbyTemperature = STANDBY_TEMPERATURE_DEG * 10;
            wavinController.writeRegister(WavinController::CATEGORY_PACKED_DATA, channel, WavinController::PACKED_DATA_STANDBY_TEMPERATURE, standbyTemperature);
            publishConfiguration(channel);
          }

          // Read the current setpoint programmed for channel
          if (wavinController.readRegisters(WavinController::CATEGORY_PACKED_DATA, channel, WavinController::PACKED_DATA_MANUAL_TEMPERATURE, 1, registers))
          {
            uint16_t setpoint = registers[0];

            String topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_SETPOINT_GET);
            String payload = temperatureAsFloatString(setpoint);

            publishIfNewValue(topic, payload, setpoint, &(lastSentValues[channel].setpoint));
          }

          // Read the current mode for the channel
          if (wavinController.readRegisters(WavinController::CATEGORY_PACKED_DATA, channel, WavinController::PACKED_DATA_CONFIGURATION, 1, registers))
          {
            uint16_t mode = registers[0] & WavinController::PACKED_DATA_CONFIGURATION_MODE_MASK; 

            String topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_MODE_GET);
            if(mode == WavinController::PACKED_DATA_CONFIGURATION_MODE_STANDBY)
            {
              publishIfNewValue(topic, MQTT_VALUE_MODE_STANDBY, mode, &(lastSentValues[channel].mode));
            }
            else if(mode == WavinController::PACKED_DATA_CONFIGURATION_MODE_MANUAL)
            {
              publishIfNewValue(topic, MQTT_VALUE_MODE_MANUAL, mode, &(lastSentValues[channel].mode));
            }            
          }

          // Read the current status of the output for channel
          if (wavinController.readRegisters(WavinController::CATEGORY_CHANNELS, channel, WavinController::CHANNELS_TIMER_EVENT, 1, registers))
          {
            uint16_t status = registers[0] & WavinController::CHANNELS_TIMER_EVENT_OUTP_ON_MASK;

            String topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_OUTPUT);
            String payload;
            if (status & WavinController::CHANNELS_TIMER_EVENT_OUTP_ON_MASK)
              payload = "on";
            else
              payload = "off";

            publishIfNewValue(topic, payload, status, &(lastSentValues[channel].status));
          }

          // If a thermostat for the channel is connected to the controller
          if(!allThermostatsLost)
          {
            // Read values from the primary thermostat connected to this channel 
            // Primary element from controller is returned as index+1, so 1 i subtracted here to read the correct element
            if (wavinController.readRegisters(WavinController::CATEGORY_ELEMENTS, primaryElement-1, 0, 11, registers))
            {
              uint16_t temperature = registers[WavinController::ELEMENTS_AIR_TEMPERATURE];
              uint16_t battery = registers[WavinController::ELEMENTS_BATTERY_STATUS]; // In 10% steps

              // Air temperature reading
              String topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_CURRENT);
              String payload = temperatureAsFloatString(temperature);

              publishIfNewValue(topic, payload, temperature, &(lastSentValues[channel].airtemperature));


              // Floor temperature reading
              temperature = registers[WavinController::ELEMENTS_FLOOR_TEMPERATURE];

              topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_CURRENTFLOOR);
              payload = temperatureAsFloatString(temperature);

              publishIfNewValue(topic, payload, temperature, &(lastSentValues[channel].floortemperature));


              // Humidity reading
              uint16_t humidity = registers[WavinController::ELEMENTS_REL_HUMIDITY];

              topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_HUMIDITY);
              payload = temperatureAsFloatString(humidity); // same logic for humidity as with temperatures - just reuse

              publishIfNewValue(topic, payload, humidity, &(lastSentValues[channel].humidity));


              // Dew point temperature reading
              temperature = registers[WavinController::ELEMENTS_DEW_POINT];

              topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_DEWPOINT);
              payload = temperatureAsFloatString(temperature);

              publishIfNewValue(topic, payload, temperature, &(lastSentValues[channel].dewpoint));


              // Battery status
              topic = String(MQTT_PREFIX + mqttDeviceNameWithMac + "/" + announcedChannel + MQTT_SUFFIX_BATTERY);
              payload = String(battery*10);

              publishIfNewValue(topic, payload, battery, &(lastSentValues[channel].battery));
            }
          }         
        }

        // Process incomming messages and maintain connection to the server
        if(!mqttClient.loop())
        {
            return;
        }
      }
    }
  }
}
