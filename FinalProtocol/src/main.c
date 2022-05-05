/* Final Protocol for Team Dapper Dingo
   Collects temperature reading and battery percentage from ADC
   and then transmits to class server
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
//#include "minimal_wifi.h"
#include "esp_sleep.h"

#define BROKER_URI "mqtt://en1-pi.eecs.tufts.edu"

// struct definition for data packet
struct packet {
    int32_t sensor_temp; // Temperature in C, multiplied by 1000
    int32_t thermistor_temp; // Temp in degrees C, -2^31 is unused
    time_t time; // Epoch time
    uint8_t battery; // Percentage of battery capacity, 0 to 100, 255 means no battery or no measurement
    uint16_t data_len;
    uint8_t team_data;
};

//#define TIME_ASLEEP 3600000000
//#define TIME_ASLEEP 10000 // Stay asleep for 10 seconds for testing

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
//#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel_sensor = ADC_CHANNEL_1;     //GPIO17 for ESP32_C3
static const adc_channel_t channel_therm = ADC_CHANNEL_3;     //GPIO3 for ESP32_C3
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


RTC_DATA_ATTR static int boot_count = 0;//count reboot time from deep sleep
                                        //RTC_DATA_ATTR stores data to RTC memory and maintains its value during deep sleep

void app_main() {

    uint64_t time_asleep = 10000; // in us
    uint64_t time_asleep_test = 5000000;// 5 sec

    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    /*
    // Initialize the MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = BROKER_URI,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    */
    
    //Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel_sensor, atten);
    adc1_config_channel_atten(channel_therm, atten);

    int32_t sensor_reading=0;
    int32_t thermistor_reading=0;

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    while (1) {
    printf("boot time: %d\n", boot_count);
    boot_count++;

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

    //Print to serial FOR TESTING
    //printf("Raw: %d\tvolt: %d\tsensorTemp: %dC\n", sensor_reading, voltage_sensor, temp_sensor);
    //vTaskDelay(pdMS_TO_TICKS(50));
    //printf("Raw: %d\tvolt: %d\tthermTemp: %dC\n", thermistor_reading, voltage_therm, temp_therm);
    //vTaskDelay(pdMS_TO_TICKS(1000));

    struct packet curr_reading;
    curr_reading.sensor_temp = temp_sensor;
    curr_reading.thermistor_temp = temp_therm;
    // curr_reading.time = Update after connecting to WiFi
    curr_reading.battery = 255;
    curr_reading.data_len = 0;

    
    esp_deep_sleep(time_asleep_test);
    }
}

int32_t Convert2Temp(uint32_t volt){
    return ((volt/3300.0)*175.72-46.85)*1000;
}