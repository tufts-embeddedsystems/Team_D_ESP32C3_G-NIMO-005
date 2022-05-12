/* Final Protocol for Team Dapper Dingo
   Collects temperature reading and battery percentage from ADC
   and then transmits to class server
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
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
#include "minimal_wifi.h"
#include "esp_sleep.h"

#define BROKER_URI "mqtt://en1-pi.eecs.tufts.edu"

// struct definition for data packet
struct packet {
    int64_t time; // Epoch time
    int32_t sensor_temp; // Temperature in C, multiplied by 1000
    int32_t thermistor_temp; // Temp in degrees C, -2^31 is unused
    uint16_t battery; // Percentage of battery capacity, 0 to 100, 255 means no battery or no measurement
    uint16_t data_len;
    uint8_t team_data;
};

//#define TIME_ASLEEP 3600000000
// #define TIME_ASLEEP 10000 // Stay asleep for 10 seconds for testing

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling
#define CP_GPIO         GPIO_NUM_0           //GPIO for chip sensor power
#define THM_GPIO         GPIO_NUM_2           //GPIO for thermistor power         

static esp_adc_cal_characteristics_t *adc_chars;
//#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel_sensor = ADC_CHANNEL_1;     //GPIO 17 for ESP32_C3
//static const adc_channel_t channel_therm = ADC_CHANNEL_6;     //GPIO 17 34 for ESP32
static const adc_channel_t channel_therm = ADC_CHANNEL_3;     //GPIO 3 for ESP32_C3
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
//#elif CONFIG_IDF_TARGET_ESP32S2
//static const adc_channel_t channel = ADC_CHANNEL_6;     // GPIO7 if ADC1, GPIO17 if ADC2
//static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
//#endif
static const adc_atten_t atten = ADC_ATTEN_DB_11;
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
int32_t volt2R(uint32_t volt);
int32_t Convert2Temp_therm(uint32_t volt);

void app_main() {
    uint64_t time_asleep = 3600000000; //time in us

    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    // `ESP_ERROR_CHECK` is a macro which checks that the return value of a function is
    // `ESP_OK`.  If not, it prints some debug information and aborts the program.

    // Enable Flash (aka non-volatile storage, NVS)
    // The WiFi stack uses this to store some persistent information
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize the network software stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize the event loop, a separate thread which checks for events
    // (like WiFi being connected or receiving an MQTT message) and dispatches
    // functions to handle them.
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Now connect to WiFi
    ESP_ERROR_CHECK(example_connect());

    // Initialize the MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = BROKER_URI,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

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

    gpio_reset_pin(CP_GPIO);
    gpio_reset_pin(THM_GPIO);
    gpio_set_direction(CP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(THM_GPIO, GPIO_MODE_OUTPUT);

    while (1) {

    
    uint8_t ChipID[6];
    esp_efuse_mac_get_default(ChipID);   
    printf("%02X%02X%02X%02X%02X%02X\n",ChipID[0],ChipID[1],ChipID[2],ChipID[3],ChipID[4],ChipID[5]);
    vTaskDelay(pdMS_TO_TICKS(1000));  
    
    gpio_set_level(CP_GPIO,0);
    gpio_set_level(THM_GPIO,0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(CP_GPIO, 1);
    gpio_set_level(THM_GPIO,1);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Let sensor warm up first

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
    int32_t temp_therm = Convert2Temp_therm(voltage_therm);

    //Print to serial FOR TESTING
    printf("Raw: %d\tvolt: %d\tsensorTemp: %dC\n", sensor_reading, voltage_sensor, temp_sensor);
    vTaskDelay(pdMS_TO_TICKS(50));
    printf("Raw: %d\tvolt: %d\tthermTemp: %dC\n", thermistor_reading, voltage_therm, temp_therm);
    vTaskDelay(pdMS_TO_TICKS(1000));

    struct packet curr_reading;
    curr_reading.sensor_temp = temp_sensor;
    curr_reading.thermistor_temp = temp_therm;

    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    curr_reading.time = current_time.tv_sec;
    
    curr_reading.battery = 255;
    curr_reading.data_len = 0;
    curr_reading.team_data = 0;

    void *message = &curr_reading;
    const char *TAG = "MQTT_HANDLE";
    int msg_id = esp_mqtt_client_publish(client, "nodes/dapper-dingos/NODE1", (char *)message, 19, 0, 0); //change this for every node
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    esp_deep_sleep(time_asleep); // Enter deep sleep
    }
}

int32_t Convert2Temp(uint32_t volt){
    return ((volt/3300.0)*175.72-46.85)*1000;
}

int32_t volt2R(uint32_t volt){
    //return resistance in Ohm (for thermistor)
    return((volt*100000.0)/(3300.0-volt));
}

int32_t Convert2Temp_therm(uint32_t volt){
    //TODO: find a better way to fit the curve. 
    double R = volt2R(volt);
    double T = (-22.53*log(R)+294.27)*1000;
    return T;
}