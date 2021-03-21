#include <ESP8266WiFi.h>

const String   WIFI_SSID = "My Wi-Fi Network";         // wifi ssid
const String   WIFI_PASS = "password";     // wifi password

const String   MQTT_SERVER = "IP"; // mqtt server address without port number
const String   MQTT_USER   = "user";       // mqtt user. Use "" for no username
const String   MQTT_PASS   = "password";       // mqtt password. Use "" for no password
const uint16_t MQTT_PORT   = 1883;                             // mqtt port
