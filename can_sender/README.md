# Automotive Telematics Throttle Sensor ECU (ESP32-C3)

Firmware phát tín hiệu CAN Bus thời gian thực mô phỏng ECU chân ga ô tô (Electronic Throttle Control - ETC / TPS) truyền qua giao thức CAN 2.0B (TWAI) tới BeagleBone Black In-Vehicle Infotainment (IVI).

---

## 1. Hardware Architecture & Pinout

| ESP32-C3 Pin | Chức năng | Kết nối ngoại vi | Ghi chú |
|:---:|:---:|:---:|:---:|
| **GPIO 2** | `ADC1_CH2` | Chân giữa Biến trở (TPS) | 0 - 3.3V (12-bit ADC, dải 0 - 4095) |
| **GPIO 5** | `TWAI_TX` | Chân CTX của Transceiver VP230 | Tín hiệu phát CAN TX |
| **GPIO 6** | `TWAI_RX` | Chân CRX của Transceiver VP230 | Tín hiệu nhận CAN RX |
| **3V3 & GND** | Nguồn | Cấp cho Transceiver VP230 và Biến trở | Nguồn tham chiếu chung |

---

## 2. CAN Frame Format (Standard ID: `0x100`, DLC: 8)

| Byte | Tín hiệu | Đơn vị / Định dạng | Mô tả kỹ thuật |
|:---:|:---:|:---:|:---:|
| **Byte 0** | `TPS` | `0 - 100 %` | Độ mở bướm ga từ biến trở ADC |
| **Byte 1** | `Vehicle Speed` | `0 - 200 km/h` | Tốc độ tính toán tương ứng: `(TPS * 200) / 100` |
| **Byte 2..3** | `Engine RPM` | `800 - 6500 RPM` | Vòng tua động cơ (Big Endian, MSB = Byte 2, LSB = Byte 3) |
| **Byte 4** | `Gear Position` | `0: P, 1: R, 2: N, 3: D` | Hộp số tự động (P khi TPS=0, D khi tăng tốc) |
| **Byte 5..6** | `Reserved` | `0x00 0x00` | Dự phòng mở rộng |
| **Byte 7** | `Sequence Counter` | `0 - 255` | Bộ đếm chu kỳ giám sát toàn vẹn đường truyền |

---

## 3. Build & Flash (ESP-IDF v5.x)

```bash
# Thiết lập target chip ESP32-C3
idf.py set-target esp32c3

# Biên dịch, nạp và mở cổng monitor
idf.py build flash -p COM_PORT monitor
```
