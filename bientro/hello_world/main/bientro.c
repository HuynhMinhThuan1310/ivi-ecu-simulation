#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
static const char *TAG = "PORT_CAR";

#define POT_ADC_CHANNEL ADC_CHANNEL_6
void app_main(void){
    ESP_LOGI(TAG,"=== Start Programing Read Registor ===");
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 ={
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, POT_ADC_CHANNEL, &config));

    int adc_raw = 0;
    int speed_percent =0;
    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, POT_ADC_CHANNEL, &adc_raw));
        
        // 0% -> 100%
        speed_percent = (adc_raw * 100) / 4095;
        if (speed_percent > 100) speed_percent = 100;
        if (speed_percent < 0) speed_percent = 0;
        // Tính toán tốc độ tối đa 200 km/h và vòng tua 800 - 6500 RPM tương ứng
        int speed_kmh = (speed_percent * 200) / 100;
        int rpm = 800 + (speed_percent * (6500 - 800)) / 100;
        int power_kw = (speed_percent * 150) / 100;
        // Gửi dòng JSON qua Serial UART0 (qua cổng USB)
        // Lưu ý: Chuỗi JSON phải nằm gọn trên 1 dòng kết thúc bằng '\n'
        printf("{\"speed\": %d, \"throttle\": %d, \"rpm\": %d, \"soc\": 85, \"range_km\": 380, \"power_kw\": %d, \"temp_c\": 29, \"odometer\": 12450, \"gear\": \"D\", \"drive_mode\": \"SPORT\", \"lights\": {\"left\": false, \"right\": false, \"low\": true}, \"doors\": {\"fl\": false, \"fr\": false, \"rl\": false, \"rr\": false}, \"locked\": true}\n",
               speed_kmh, speed_percent, rpm, power_kw);
        // Cập nhật mượt mà 20 lần/giây (50ms) hoặc 10 lần/giây (100ms)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

