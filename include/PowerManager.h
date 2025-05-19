#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

class PowerManager {
private:
    static const uint8_t POWER_SENSE_PIN = 34;  // GPIO pin to monitor power input
    static constexpr float VOLTAGE_THRESHOLD = 4.5f;  // Voltage threshold for power loss detection
    static esp_adc_cal_characteristics_t adc_chars;
    static bool initialized;

public:
    static void begin() {
        pinMode(POWER_SENSE_PIN, INPUT);
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
        initialized = true;
    }

    static float getInputVoltage() {
        if (!initialized) return 0;
        uint32_t reading = 0;
        for(int i = 0; i < 10; i++) {
            reading += adc1_get_raw(ADC1_CHANNEL_6);
            delay(1);
        }
        reading /= 10;
        uint32_t voltage = esp_adc_cal_raw_to_voltage(reading, &adc_chars);
        return voltage * (5.0 / 1000.0); // Convert to actual voltage based on voltage divider
    }

    static bool isPowerLow() {
        return getInputVoltage() < VOLTAGE_THRESHOLD;
    }
};

bool PowerManager::initialized = false;
esp_adc_cal_characteristics_t PowerManager::adc_chars;

#endif // POWER_MANAGER_H
