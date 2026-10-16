#include "throttle_sensor.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#define TAG "THROTTLE_SENSOR"
#define THROTTLE_ADC_UNIT  ADC_UNIT_1
#define THROTTLE_ADC_CHAN  ADC_CHANNEL_2  // GPIO 2

static adc_oneshot_unit_handle_t adc_handle;

void throttle_sensor_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = THROTTLE_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, THROTTLE_ADC_CHAN, &chan_config));
    ESP_LOGI(TAG, "Khoi tao ADC thanh cong tren GPIO 2");
}

throttle_data_t throttle_sensor_read(void) {
    throttle_data_t data;
    int raw = 0;
    adc_oneshot_read(adc_handle, THROTTLE_ADC_CHAN, &raw);

    data.raw_adc = raw;

    // 1. Tính % chân ga (0 - 100%)
    data.throttle_pct = (raw * 100) / 4095;
    if (data.throttle_pct > 100) data.throttle_pct = 100;
    if (data.throttle_pct < 0)   data.throttle_pct = 0;

    // 2. Tính vận tốc xe (0 - 200 km/h)
    data.speed_kmh = (data.throttle_pct * 200) / 100;

    // 3. Tính vòng tua máy (800 - 6500 RPM)
    data.rpm = 800 + (data.throttle_pct * (6500 - 800)) / 100;

    return data;
}