#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>

SensirionI2cScd4x scd4x;

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(100); }

    Wire.begin(6, 7);  // SDA=GPIO6, SCL=GPIO7 (Xiao ESP32-C3)
    scd4x.begin(Wire, SCD40_I2C_ADDR_62);

    int16_t error;
    char errorMessage[256];

    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.printf("stopPeriodicMeasurement error: %s\n", errorMessage);
    }
    delay(500);

    error = scd4x.startPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.printf("startPeriodicMeasurement error: %s\n", errorMessage);
        return;
    }

    Serial.println("SCD40 initialized. Waiting for first measurement (5s)...");
}

void loop() {
    delay(1000);

    int16_t error;
    char errorMessage[256];
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;

    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.printf("readMeasurement error: %s\n", errorMessage);
        return;
    }

    Serial.printf("CO2: %4d ppm, Temp: %5.2f C, Humidity: %5.2f %%\n",
                  co2, temperature, humidity);
}
