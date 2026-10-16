#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "throttle_sensor.h"

#define TAG "MAIN_APP"
#define CAN_TX_PIN   GPIO_NUM_5
#define CAN_RX_PIN   GPIO_NUM_6
#define CAN_MSG_ID   0x100

/* Khởi tạo CAN TWAI 125 kbps */
static void can_init(void) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "CAN khoi dong thanh cong tai 125 kbps");
}

void app_main(void) {
    // 1. Khởi tạo từng module độc lập
    throttle_sensor_init();
    can_init();

    uint8_t seq = 0;

    // 2. Vòng lặp định kỳ đọc cảm biến -> gửi CAN
    while (1) {
        throttle_data_t throttle = throttle_sensor_read();

        // Đóng gói frame CAN 8 bytes
        twai_message_t msg;
        msg.identifier = CAN_MSG_ID;
        msg.extd = 0;
        msg.rtr = 0;
        msg.data_length_code = 8;
        msg.data[0] = (uint8_t)throttle.throttle_pct;
        msg.data[1] = (uint8_t)throttle.speed_kmh;
        msg.data[2] = (uint8_t)(throttle.rpm >> 8);
        msg.data[3] = (uint8_t)(throttle.rpm & 0xFF);
        msg.data[4] = (throttle.speed_kmh == 0) ? 0 : 3; // 0=P, 3=D
        msg.data[5] = 0;
        msg.data[6] = 0;
        msg.data[7] = seq++;

        twai_transmit(&msg, pdMS_TO_TICKS(10));

        ESP_LOGI(TAG, "Ga: %3d%% | Toc do: %3d km/h | RPM: %4d", 
                 throttle.throttle_pct, throttle.speed_kmh, throttle.rpm);

        vTaskDelay(pdMS_TO_TICKS(50)); // 20 Hz
    }
}