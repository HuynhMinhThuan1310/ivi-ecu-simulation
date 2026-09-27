# 🚗 SMART CAR-IVI & TELEMATICS GATEWAY EMBEDDED LINUX

Dự án phát triển hệ sinh thái **In-Vehicle Infotainment (Car-IVI)** và **Bộ điều khiển Trung tâm (Central Telematics Gateway)** chuẩn Automotive trên nền tảng **BeagleBone Black (TI AM3358 ARM Cortex-A8, Linux Kernel 6.6.58)** và vi điều khiển **ESP32**.

Dự án áp dụng phương pháp lập trình Linux nhúng chuyên sâu (Embedded Linux Kernel Driver & Native C Architecture), tối ưu hóa tài nguyên thời gian thực và không phụ thuộc vào các runtime nặng.

---

## 🏛️ Lộ trình 5 Bước Chuyên sâu (Automotive Curriculum)

| Bước | Nội dung | Module / Driver | Trạng thái |
|:---:|---|---|:---:|
| **1** | **Character Device Driver & IOCTL** | `/dev/mydev` (`step1_chardev`) | **✅ 100% Hoàn thành** |
| **2** | **GPIO & Ngắt phần cứng (Wheel Speed Sensor)** | `/dev/speed_dev` (`step2_gpio_interrupt`) | **✅ 100% Hoàn thành** |
| **3** | **Device Tree & Platform Driver (Chuẩn Automotive)** | `speed_platform_driver` & `speed_sensor.dts` (`step3_platform`) | **✅ 100% Hoàn thành** |
| **4** | **Giao tiếp UART với ESP32 Telematics** | Termios UART RX/TX Protocol (`step4_uart`) | ⏳ Sắp thực hiện |
| **5** | **Giao diện Táp-lô LVGL trên Framebuffer & Touch** | DRM/KMS Framebuffer & `evdev` Touch | ⏳ Sắp thực hiện |

---

## 📁 Cấu trúc Thư mục Dự án

```
c:\prj-bbb\
├── step1_chardev/               # BƯỚC 1: Character Device Driver & IOCTL
│   ├── hello.c                  # Driver đăng ký cdev, open/read/write/ioctl
│   ├── test_ioctl.c             # App User Space test lệnh IOCTL GET/SET
│   └── Makefile                 # Makefile cross-compile bằng Toolchain Buildroot
│
├── step2_gpio_interrupt/        # BƯỚC 2: GPIO & Ngắt Phần cứng (Đo Tốc độ Xe)
│   ├── speed_driver.c           # Driver ngắt GPIO 540, debounce ktime, atomic, poll
│   ├── test_speed.c             # App táp-lô User Space ngủ 0% CPU, thức dậy khi có xung
│   └── Makefile                 # Makefile tự động build và deploy scp -O sang BBB
│
├── step3_platform/              # BƯỚC 3: Device Tree & Platform Driver Chuẩn Công Nghiệp
│   ├── speed_sensor.dts         # Bản vẽ phần cứng Device Tree (chân Bank 1, pin 28)
│   ├── speed_platform_driver.c  # Platform Driver tự động probe() ghép đôi với Device Tree
│   └── Makefile                 # Makefile biên dịch dtc -> .dtbo và driver -> .ko
│
├── public/                      # Tài nguyên giao diện tĩnh & tài liệu bổ trợ
├── .gitignore                   # Cấu hình bỏ qua file rác và build artifacts
└── README.md                    # Tài liệu kỹ thuật chi tiết toàn bộ dự án
```

---

## ⚡ BƯỚC 1: Character Device Driver & IOCTL (`step1_chardev`)

### 1. Mục tiêu
* Làm chủ chu trình vòng đời của Character Device Driver trong Linux Kernel: cấp phát `dev_t` động (`alloc_chrdev_region`), đăng ký `cdev`, tự động sinh Device Node `/dev/mydev` qua `class_create` và `device_create`.
* Thực hiện an toàn việc trao đổi dữ liệu giữa User Space và Kernel Space bằng `copy_to_user` và `copy_from_user`.
* Thiết kế kênh điều khiển phần cứng chuyên biệt thông qua giao thức **IOCTL** chuẩn POSIX (`_IOW`, `_IOR`).

### 2. Định nghĩa mã lệnh IOCTL
```c
#define MYDEV_MAGIC 'k'
#define MYDEV_SET_VAL _IOW(MYDEV_MAGIC, 1, int) // Gửi cấu hình vào Kernel
#define MYDEV_GET_VAL _IOR(MYDEV_MAGIC, 2, int) // Đọc trạng thái từ Kernel ra
```

### 3. Kết quả Kiểm thử Thực tế
```text
# insmod hello.ko
[  880.192124] mydev: Khoi tao kem IOCTL thanh cong! Major = 238, Minor = 0
# ./test_ioctl
[USER] Gửi lệnh IOCTL SET gia tri = 2026 vao Kernel...
[  890.715091] mydev: [IOCTL] Da dat gia tri moi = 2026
[USER] Gửi lệnh IOCTL GET gia tri tu Kernel ve...
[  890.725648] mydev: [IOCTL] Da doc gia tri = 2026
[USER] Ket qua nhan duoc: 2026
🎉 TEST IOCTL THANH CONG 100%!
```

---

## 🏎️ BƯỚC 2: GPIO & Ngắt Phần cứng - Wheel Speed Sensor (`step2_gpio_interrupt`)

### 1. Mục tiêu & Nguyên lý Hoạt động
* **Mô phỏng Cảm biến Tốc độ Bánh xe:** Mỗi vòng quay của bánh xe kích hoạt 1 xung sườn âm (Falling Edge: 3.3V $\rightarrow$ 0V) trên chân GPIO.
* **Xử lý ngắt thời gian thực (ISR):** CPU không chạy vòng lặp `while(1)` canh chân GPIO mà hoàn toàn rảnh rỗi. Khi có xung, phần cứng AM3358 tự ngắt CPU để tăng biến đếm `atomic_t pulse_count`.
* **Lọc rung tiếp điểm cơ học (Debounce):** Dùng đồng hồ nano-giây `ktime_get()` và `ktime_to_ms()`, triệt tiêu toàn bộ các xung nhiễu nảy kim loại có chu kỳ $< 100\text{ms}$.
* **Kiến trúc Bất đồng bộ Event-driven (`wait_queue` & `poll`):**
  * Ứng dụng User Space (`test_speed.c`) gọi `poll()` và **ngủ sâu (tiêu thụ đúng 0% CPU)** khi xe đứng yên.
  * Khi bánh xe quay $\rightarrow$ Hàm ngắt gọi `wake_up_interruptible(&speed_waitqueue)` $\rightarrow$ Kernel đánh thức User Space dậy ngay lập tức để cập nhật tốc độ lên màn hình.

### 2. Sơ đồ Đấu nối Phần cứng (Header P9 - BeagleBone Black)

```
        HEADER P9 (BeagleBone Black)
       ┌──────────┬──────────┐
  GND  │  P9_01   │  P9_02   │  GND
 3.3V  │  P9_03   │  P9_04   │  3.3V
       │  P9_05   │  P9_06   │
       │  P9_07   │  P9_08   │
       │  P9_09   │  P9_10   │
       │  P9_11   │  P9_12   │ <--- GPIO 540 (GPIO1_28) - Bắt xung ngắt
       │   ...    │   ...    │
       └──────────┴──────────┘
```

* **Chân P9_12:** Ánh xạ vào `platform/4804c000.gpio` (cụm GPIO1), offset 28. Trên Linux Kernel 6.6, số hiệu GPIO được cấp phát là **540** ($512 + 28 = 540$).
* **Thanh ghi cấu hình Pinmux:** Địa chỉ `0x44E10878` (`CONF_GPMC_BEN1`) = `0x00000037` (Chế độ Input Mode 7, Receiver Enabled, có sẵn trở kéo lên nội Pull-up 3.3V).

### 3. Kết quả Kiểm thử Thực tế
```text
=========================================
   CAR-IVI SPEEDOMETER TEST (STEP 2)
=========================================
[USER SPACE] Dang o che do NGU YEN cho banh xe quay (0% CPU)...

[ 2718.266557] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 1
🚗 [BANH XE QUAY] Xung thu:   1 | Toc do mo phong:   1.5 km/h
[ 2719.366600] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 2
🚗 [BANH XE QUAY] Xung thu:   2 | Toc do mo phong:   3.0 km/h
...
[ 2746.696591] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 14
🚗 [BANH XE QUAY] Xung thu:  14 | Toc do mo phong:  21.0 km/h
[ 2747.160513] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 15
🚗 [BANH XE QUAY] Xung thu:  15 | Toc do mo phong:  22.5 km/h
```

---

## 🏗️ BƯỚC 3: Device Tree & Platform Driver (`step3_platform`)

### 1. Ý nghĩa & Kiến trúc Phân tách Phần cứng / Phần mềm
* **Vấn đề của Bước 2:** Số chân `540` bị gán chết (hardcode) trong code C. Nếu đổi chân cắm hoặc mang driver sang bo mạch khác, driver bị sập.
* **Giải pháp chuẩn Automotive:**
  * **Device Tree (`speed_sensor.dts`):** File mô tả phần cứng độc lập. Khai báo nút `car_speed_sensor` với thuộc tính `speed-gpios = <&gpio1 28 1>;` và chuỗi nhận diện `compatible = "car-ivi,speed-sensor";`.
  * **Platform Driver (`speed_platform_driver.c`):** Đăng ký bảng mai mối `speed_of_match[]`. Không hardcode số chân!
  * **Cặp hàm `probe()` và `remove()`:** Khi Kernel tìm thấy nút phần cứng khớp chuỗi `compatible`, Kernel tự động gọi hàm `probe()`, cấp phát chân qua `gpiod_get(dev, "speed", ...)` và ánh xạ số ngắt qua `gpiod_to_irq()`.

### 2. File Bản vẽ Phần cứng (`speed_sensor.dts`)
```dts
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            car_speed_sensor {
                compatible = "car-ivi,speed-sensor";
                speed-gpios = <&gpio1 28 1>; /* Bank 1, chân 28 (P9_12), Active-Low */
                status = "okay";
            };
        };
    };
};
```

### 3. Ghép nối Cây thiết bị bằng `fdtoverlay`
```bash
# Biên dịch DTS sang DTBO
dtc -O dtb -o speed_sensor.dtbo -b 0 -@ speed_sensor.dts

# Ghép vĩnh viễn vào Device Tree gốc am335x-boneblack.dtb
fdtoverlay -i am335x-boneblack.dtb -o am335x-boneblack.dtb speed_sensor.dtbo
```

### 4. Kết quả Kiểm thử Thực tế trên BeagleBone Black
```text
# ls -d /sys/firmware/devicetree/base/car_speed_sensor
/sys/firmware/devicetree/base/car_speed_sensor

# insmod speed_platform_driver.ko
[   52.143670] speed_platform_driver: loading out-of-tree module taints kernel.
[   52.151995] car_speed_sensor car_speed_sensor: speed_platform: Bat tay phan cung thanh cong! Bat dau khoi tao...
[   52.165239] car_speed_sensor car_speed_sensor: Kernel tu cap phat IRQ so: 63 cho cam bien toc do
[   52.174845] car_speed_sensor car_speed_sensor: Platform Driver da san sang bat xung!
```

---

## 🔮 BƯỚC 4: Giao tiếp UART với ESP32 Telematics Gateway (Kế hoạch Chi tiết)

### 1. Bối cảnh Hệ thống
Trong ô tô thực tế, vi điều khiển **ESP32** đóng vai trò là **Body/Sensor ECU** (thu thập cảm biến bàn đạp ga TPS, góc lái, trạng thái phanh), còn **BeagleBone Black** đóng vai trò là **Head Unit / Telematics Gateway**. Hai bên giao tiếp tốc độ cao qua đường truyền nối tiếp UART chuẩn Automotive.

### 2. Kế hoạch Thực hiện:
1. **Phần cứng:**
   * Sử dụng cổng **UART4** trên BeagleBone Black (Chân `P9_11` RX và `P9_13` TX).
   * Nối chéo với chân UART của ESP32 (TX $\rightarrow$ RX, RX $\rightarrow$ TX, GND chung).
2. **Firmware ESP32:**
   * Đọc biến trở chân ga (ADC 0 - 3.3V).
   * Đóng gói khung truyền dữ liệu thời gian thực (Packet Framing có Byte mở đầu `0xAA`, mã lệnh, dữ liệu tốc độ/vòng tua, và Checksum kiểm tra lỗi).
3. **Chương trình C Linux trên BeagleBone Black (`car_telematics_uart.c`):**
   * Sử dụng thư viện chuẩn POSIX **`termios`** điều khiển cổng `/dev/ttyS4` ở tốc độ `115200 8N1`.
   * Chế độ Non-canonical (đọc dữ liệu theo từng byte không chờ Enter).
   * Giải mã gói tin và xác thực Checksum, đưa dữ liệu vào hệ thống táp-lô.
