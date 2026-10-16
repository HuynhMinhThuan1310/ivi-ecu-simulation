#ifndef THROTTLE_SENSOR_H
#define THROTTLE_SENSOR_H

#include <stdint.h>

/* Cấu trúc dữ liệu cảm biến chân ga */
typedef struct {
    int raw_adc;       // Giá trị ADC thô (0 - 4095)
    int throttle_pct;  // % Bàn đạp ga (0 - 100%)
    int speed_kmh;     // Vận tốc mô phỏng (0 - 200 km/h)
    int rpm;           // Vòng tua máy (800 - 6500 RPM)
} throttle_data_t;

/* Khởi tạo ADC kênh GPIO 2 */
void throttle_sensor_init(void);

/* Đọc dữ liệu chân ga đã qua xử lý */
throttle_data_t throttle_sensor_read(void);

#endif // THROTTLE_SENSOR_H