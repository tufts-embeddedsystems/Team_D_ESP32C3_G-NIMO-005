/* Final Protocol for Team Dapper Dingo
   Collects temperature reading and battery percentage from ADC
   and then transmits to class server
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "esp_wifi.h"
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

#define TIME_ASLEEP 3600000000

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
//#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel_sensor = ADC_CHANNEL_1;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t channel_therm = ADC_CHANNEL_3;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
//#elif CONFIG_IDF_TARGET_ESP32S2
//static const adc_channel_t channel = ADC_CHANNEL_6;     // GPIO7 if ADC1, GPIO17 if ADC2
//static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
//#endif
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
//#error "This example is configured for ESP32/ESP32S2."
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

int32_t Convert2Temp(uint32_t volt);

void app_main() {
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    
    struct packet packets[10]; // Array of packets to store data 

    //Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel_sensor, atten);
    adc1_config_channel_atten(channel_therm, atten);
    //esp_err_t esp_sleep_enable_timer_wakeup((uint64_t) TIME_ASLEEP); // Enable sleep mode timer

    int32_t sensor_reading=0;
    int32_t thermistor_reading=0;

    uint8_t count = 0; // Amount of packets stored in current batch

    while (1) {

    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        sensor_reading += adc1_get_raw((adc1_channel_t)channel_sensor);
        thermistor_reading += adc1_get_raw((adc1_channel_t)channel_therm);
    }
    sensor_reading /= NO_OF_SAMPLES;
    thermistor_reading /= NO_OF_SAMPLES;

    //Convert adc_reading to voltage in mV
    uint32_t voltage_sensor = esp_adc_cal_raw_to_voltage(sensor_reading, adc_chars);
    uint32_t voltage_therm = esp_adc_cal_raw_to_voltage(thermistor_reading, adc_chars);
    int32_t temp_sensor = Convert2Temp(voltage_sensor);
    int32_t temp_therm = Convert2Temp(voltage_therm);

    printf("Raw: %d\tsensorTemp: %dC\n", sensor_reading, temp_sensor);
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("Raw: %d\tthermTemp: %dC\n", thermistor_reading, temp_therm);
    vTaskDelay(pdMS_TO_TICKS(1000));

    packets[count].sensor_temp = temp_sensor;
    packets[count].thermistor_temp = temp_therm;
    //packets[count].time
    packets[count].battery = 255;
    packets[count].data_len = 0;

    count++;
        if (count == 10) {
            // DO MQTT

            count = 0;
        }
        //esp_deep_sleep_start(); // Enter deep sleep
    }
}

int32_t Convert2Temp(uint32_t volt){
    return ((volt/3300)*175.72-46.85)*1000;
}