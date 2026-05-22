#ifndef CONFIG_H
#define CONFIG_H

// WiFi (set your own credentials)
constexpr const char* WIFI_SSID = "your-ssid";
constexpr const char* WIFI_PASSWORD = "your-password";

// NTP
constexpr const char* NTP_SERVER1 = "ntp.nict.jp";
constexpr const char* NTP_SERVER2 = "time.google.com";
constexpr const long   TIMEZONE_OFFSET = 9 * 3600;  // JST = UTC+9

// I2C
constexpr uint8_t I2C_SDA = 6;
constexpr uint8_t I2C_SCL = 7;

// OLED (SSD1306)
constexpr uint8_t OLED_ADDR = 0x3C;

// MQTT
constexpr const char* MQTT_SERVER = "aosy.f5.si";
constexpr int         MQTT_PORT = 1883;
constexpr const char* MQTT_TOPIC_CO2 = "sensors/esp32/co2";
constexpr const char* MQTT_TOPIC_TEMP = "sensors/esp32/temperature";
constexpr const char* MQTT_TOPIC_HUM = "sensors/esp32/humidity";
constexpr unsigned long MQTT_INTERVAL_MS = 60000;

#endif
