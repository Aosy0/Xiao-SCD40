#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <SSD1306Wire.h>
#include <SensirionI2cScd4x.h>
#include <PubSubClient.h>
#include "config.h"

SensirionI2cScd4x scd4x;
SSD1306Wire display(OLED_ADDR, I2C_SDA, I2C_SCL);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static uint16_t lastCo2 = 0;
static float lastTemp = 0.0f;
static float lastHum = 0.0f;
static bool lastSensorOk = false;

void mqttReconnect() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (mqttClient.connected()) return;

    Serial.print("MQTT connecting...");
    String clientId = "ESP32C3-";
    clientId += WiFi.macAddress();

    if (mqttClient.connect(clientId.c_str())) {
        Serial.println("connected");
    } else {
        Serial.printf(" failed (rc=%d)\n", mqttClient.state());
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Setup Start ===");

    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.printf("I2C: SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);

    // --- SCD40 ---
    scd4x.begin(Wire, SCD40_I2C_ADDR_62);

    int16_t error;
    char errorMessage[256];

    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.printf("SCD40 stop: %s\n", errorMessage);
    } else {
        Serial.println("SCD40 stopped");
    }
    delay(500);

    error = scd4x.startPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.printf("SCD40 start: %s\n", errorMessage);
    } else {
        Serial.println("SCD40 started");
    }

    // --- OLED ---
    Serial.print("OLED init... ");
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.clear();
    display.drawString(0, 0, "Booting...");
    display.display();
    Serial.println("done");

    // --- WiFi ---
    Serial.printf("WiFi connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    display.clear();
    display.drawString(0, 0, "WiFi...");
    display.display();

    int wifiRetry = 0;
    while (WiFi.status() != WL_CONNECTED && wifiRetry < 30) {
        delay(500);
        Serial.print(".");
        wifiRetry++;
    }
    Serial.printf("\nWiFi status: %d (connected=%d)\n",
                  WiFi.status(), WL_CONNECTED);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi OK. IP: %s\n",
                      WiFi.localIP().toString().c_str());

        display.clear();
        display.drawString(0, 0, "NTP sync...");
        display.display();

        configTzTime(TIMEZONE_STR, NTP_SERVER1, NTP_SERVER2);

        struct tm timeinfo;
        bool ntpOk = false;
        for (int i = 0; i < 5; i++) {
            delay(1000);
            ntpOk = getLocalTime(&timeinfo);
            Serial.printf("NTP attempt %d: %s\n", i + 1, ntpOk ? "OK" : "waiting");
            if (ntpOk) break;
        }

        if (ntpOk) {
            char buf[64];
            strftime(buf, sizeof(buf), "Time: %Y/%m/%d %H:%M:%S", &timeinfo);
            Serial.println(buf);

            // --- Debug: compare UTC and local time ---
            time_t now_t = time(nullptr);
            struct tm timeinfo_utc;
            gmtime_r(&now_t, &timeinfo_utc);
            char buf_utc[64];
            strftime(buf_utc, sizeof(buf_utc), "%H:%M:%S", &timeinfo_utc);
            Serial.printf("DEBUG: epoch=%ld  UTC=%s  JST(local)=%s  tz=%s\n",
                          (long)now_t, buf_utc, buf, TIMEZONE_STR);

            time_t test_t = 0;
            struct tm test_tm;
            localtime_r(&test_t, &test_tm);
            strftime(buf, sizeof(buf), "%H:%M:%S", &test_tm);
            Serial.printf("DEBUG: epoch 0 localtime = %s (expect 09:00:00 for JST)\n", buf);
        } else {
            Serial.println("NTP FAILED");
        }

        mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
        Serial.printf("MQTT server: %s:%d\n", MQTT_SERVER, MQTT_PORT);
        mqttReconnect();
    } else {
        Serial.println("WiFi FAILED");
    }

    Serial.println("=== Setup Complete ===\n");
}

void loop() {
    static unsigned long prevSensorMs = 0;
    static unsigned long prevDisplayMs = 0;
    static unsigned long prevMqttKeepMs = 0;
    static unsigned long prevMqttSendMs = 0;
    unsigned long now = millis();

    // --- Read SCD40 every 5s ---
    if (now - prevSensorMs >= 5000) {
        prevSensorMs = now;

        uint16_t co2 = 0;
        float temperature = 0.0f;
        float humidity = 0.0f;
        int16_t error;
        char errorMessage[256];

        error = scd4x.readMeasurement(co2, temperature, humidity);

        if (error) {
            errorToString(error, errorMessage, 256);
            Serial.printf("SCD40 error: %s\n", errorMessage);
            lastSensorOk = false;
        } else {
            lastCo2 = co2;
            lastTemp = temperature;
            lastHum = humidity;
            lastSensorOk = true;

            Serial.printf("CO2: %4u ppm, Temp: %5.2f C, Hum: %5.2f %%\n",
                          co2, temperature, humidity);
        }
    }

    // --- MQTT keepalive ---
    mqttClient.loop();
    if (now - prevMqttKeepMs >= 30000) {
        prevMqttKeepMs = now;
        if (!mqttClient.connected()) {
            mqttReconnect();
        }
    }

    // --- MQTT send every MQTT_INTERVAL_MS ---
    if (lastSensorOk && now - prevMqttSendMs >= MQTT_INTERVAL_MS) {
        prevMqttSendMs = now;

        char jsonPayload[64];

        snprintf(jsonPayload, sizeof(jsonPayload),
                 "{\"value\": %.0f}", (float)lastCo2);
        Serial.printf("MQTT send %s: %s %s\n",
                      MQTT_TOPIC_CO2, jsonPayload,
                      mqttClient.publish(MQTT_TOPIC_CO2, jsonPayload) ? "OK" : "FAIL");

        snprintf(jsonPayload, sizeof(jsonPayload),
                 "{\"value\": %.1f}", lastTemp);
        Serial.printf("MQTT send %s: %s %s\n",
                      MQTT_TOPIC_TEMP, jsonPayload,
                      mqttClient.publish(MQTT_TOPIC_TEMP, jsonPayload) ? "OK" : "FAIL");

        snprintf(jsonPayload, sizeof(jsonPayload),
                 "{\"value\": %.1f}", lastHum);
        Serial.printf("MQTT send %s: %s %s\n",
                      MQTT_TOPIC_HUM, jsonPayload,
                      mqttClient.publish(MQTT_TOPIC_HUM, jsonPayload) ? "OK" : "FAIL");
    }

    // --- Update display every 1s ---
    if (now - prevDisplayMs >= 1000) {
        prevDisplayMs = now;

        struct tm timeinfo;
        char timeStr[9] = "--:--:--";
        if (getLocalTime(&timeinfo)) {
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        }

        display.clear();
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, 0, timeStr);
        display.setFont(ArialMT_Plain_10);

        if (lastSensorOk) {
            char buf[32];
            snprintf(buf, sizeof(buf), "CO2: %4u ppm", lastCo2);
            display.drawString(0, 18, buf);
            snprintf(buf, sizeof(buf), "Temp: %5.1f C", lastTemp);
            display.drawString(0, 32, buf);
            snprintf(buf, sizeof(buf), "Hum: %5.1f %%", lastHum);
            display.drawString(0, 46, buf);
        }
        display.display();
    }
}
