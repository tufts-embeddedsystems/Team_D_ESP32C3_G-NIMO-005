/* Final Protocol for Team Dapper Dingo
   Collects temperature reading and battery percentage from ADC
   and then transmits to class server
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

// struct definition for data packet
struct packet {
    int32_t sensor_temp; // Temperature in C, multiplied by 1000
    int32_t thermistor_temp; // Temp in degrees C, -2^31 is unused
    time_t time; // Epoch time
    uint8_t battery; // Percentage of battery capacity, 0 to 100, 255 means no battery or no measurement
    uint16_t data_len;
    uint8_t team_data;
};

#define const uint64_t TIME_ASLEEP 3600000000

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
#elif CONFIG_IDF_TARGET_ESP32S2
static const adc_channel_t channel = ADC_CHANNEL_6;     // GPIO7 if ADC1, GPIO17 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
#endif
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;

static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void app_main() {
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    
    struct packet packets[10]; // Array of packets to store data 

    esp_err_t esp_sleep_enable_timer_wakeup(TIME_ASLEEP); // Enable sleep mode timer

    uint8_t count = 0; // Amount of packets stored in current batch
    while(1) {
        int32_t sensor_reading;
        int32_t thermistor_reading;
        uint8_t battery;
        
        count++;
        if (count == 10) {
            // DO MQTT

            count = 0;
        }

        esp_deep_sleep_start(); // Enter deep sleep
    }
}