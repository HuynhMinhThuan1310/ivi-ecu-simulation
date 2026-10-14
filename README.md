# Hướng dẫn Thực hành Lập trình Linux Nhúng trên BeagleBone Black & ESP32

Dự án thực hành lập trình nhúng từ mức thanh ghi, Kernel Driver đến ứng dụng User Space trên bo mạch **BeagleBone Black** (chip TI AM3358, nhân Linux Kernel 6.6.58 build từ Buildroot) và vi điều khiển **ESP32-C3**.

Tài liệu này ghi chép lại các bước thực hiện chi tiết, sơ đồ đấu nối, câu lệnh biên dịch và kết quả kiểm tra thực tế cho từng bài lab.

---

## Cấu trúc Thư mục

```text
c:\prj-bbb\
├── step1_chardev/               # Bài 1: Character Device Driver & IOCTL
│   ├── hello.c                  # Mã nguồn driver /dev/mydev
│   ├── test_ioctl.c             # Ứng dụng User Space test lệnh IOCTL
│   └── Makefile                 # File build cross-compile
│
├── step2_gpio_interrupt/        # Bài 2: Ngắt GPIO & Đếm xung tốc độ xe
│   ├── speed_driver.c           # Driver bắt ngắt sườn âm, debounce, waitqueue/poll
│   ├── test_speed.c             # Ứng dụng đọc tốc độ thời gian thực (0% CPU)
│   └── Makefile
│
├── step3_platform/              # Bài 3: Device Tree Overlay & Platform Driver
│   ├── speed_sensor.dts         # File Device Tree mô tả chân phần cứng
│   ├── speed_platform_driver.c  # Driver sử dụng hàm probe/remove chuẩn Linux
│   └── Makefile
│
├── touch_gui/                   # Module Đồ họa Framebuffer & Cảm ứng đa điểm
│   ├── touch_gui.c              # Vẽ trực tiếp lên /dev/fb0 và đọc /dev/input/event1
│   └── Makefile
│
├── .gitignore
└── README.md
```

---

## Môi trường & Thiết lập Chung

* **Máy ảo Ubuntu (VMware):** Dùng để biên dịch chéo (Cross-compile).
  * Bộ công cụ biên dịch (Toolchain): `/home/thuan131004/bbb_workspace/buildroot/output/host/bin/arm-linux-`
  * Cây mã nguồn Kernel: `/home/thuan131004/bbb_workspace/buildroot/output/build/linux-6.6.58`
  * Công cụ biên dịch Device Tree: `dtc` và `fdtoverlay` nằm trong thư mục host của Buildroot.
* **BeagleBone Black (BBB):**
  * Kết nối máy tính qua cáp USB Mini (mạng `usb0`): IP board là `192.168.7.2`, IP máy tính là `192.168.7.1`.
  * Đăng nhập console qua cổng nối tiếp UART debug (CH340 - 115200 baud, 8N1).
  * Chuyển file từ Ubuntu sang board: Dùng lệnh `scp -O` (cờ `-O` bắt buộc cho SSH Dropbear thu nhỏ trên board).

---

## Bài 1: Character Device Driver & Test Lệnh IOCTL (`step1_chardev`)

### 1. Mục đích
* Tạo file thiết bị `/dev/mydev` bằng hàm `alloc_chrdev_region`, `cdev_init`, `cdev_add`, `class_create` và `device_create`.
* Trao đổi dữ liệu an toàn giữa Kernel và User Space qua 2 hàm `copy_from_user` và `copy_to_user`.
* Viết kênh cấu hình riêng qua giao thức **IOCTL** bằng 2 lệnh:
  * `MYDEV_SET_VAL`: Ghi một giá trị nguyên từ User Space vào biến trong Kernel.
  * `MYDEV_GET_VAL`: Đọc giá trị nguyên đó từ Kernel về User Space.

### 2. Các bước biên dịch và nạp chạy

**Trên máy ảo Ubuntu:**
```bash
cd ~/car-ivi/step1_chardev
make
make deploy   # Bắn hello.ko và test_ioctl sang board qua scp -O
```

**Trên BeagleBone Black:**
```bash
cd /root
insmod hello.ko
./test_ioctl
```

### 3. Kết quả chạy thực tế
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

## Bài 2: Ngắt Phần Cứng GPIO Đo Tốc Độ Bánh Xe (`step2_gpio_interrupt`)

### 1. Mục đích & Nguyên lý
* Mô phỏng cảm biến tốc độ bánh xe bằng một sợi dây cắm (Jumper wire):
  * Chân `P9_12` (GPIO 540) có trở kéo lên nội 3.3V.
  * Mỗi khi quẹt đầu dây vào chân `P9_01` (GND) $\rightarrow$ Điện áp rơi từ 3.3V xuống 0V (Sườn âm - Falling Edge) $\rightarrow$ Kích hoạt ngắt phần cứng (ISR).
* **Lọc rung cơ học (Debounce):** Dùng `ktime_get()` và `ktime_sub()`. Nếu 2 lần ngắt cách nhau $< 100\text{ms}$, coi là tia lửa nhiễu và bỏ qua (`return IRQ_HANDLED`).
* **Đếm nguyên tử (`atomic_t`):** Dùng `atomic_inc()` để biến đếm xung an toàn trong môi trường ngắt.
* **Cơ chế ru ngủ & đánh thức (`wait_queue` & `poll`):**
  * Ứng dụng ngoài User Space gọi `poll()` sẽ rơi vào trạng thái ngủ sâu (**0% CPU**).
  * Khi quẹt dây có xung ngắt, hàm ngắt gọi `wake_up_interruptible(&speed_waitqueue)` để đánh thức app dậy đọc số xung mới và in tốc độ ra màn hình.

### 2. Sơ đồ cắm dây Header P9
* Cắm 1 đầu dây vào chân **`P9_12`** (Hàng 6, cột phía trong bo mạch).
* Đầu còn lại cầm trên tay, chạm nhẹ vào chân **`P9_01`** (GND - Hàng 1, cột ngoài cùng) rồi nhấc ra để tạo xung.

### 3. Các bước biên dịch và nạp chạy

**Trên máy ảo Ubuntu:**
```bash
cd ~/car-ivi/step2_gpio_interrupt
make
make deploy
```

**Trên BeagleBone Black:**
```bash
cd /root
rmmod speed_driver 2>/dev/null
insmod speed_driver.ko

# Chạy app đo tốc độ
chmod +x test_speed
./test_speed
```

### 4. Kết quả chạy thực tế
```text
=========================================
   CAR-IVI SPEEDOMETER TEST (STEP 2)
=========================================
[USER SPACE] Dang o che do NGU YEN cho banh xe quay (0% CPU)...

[ 2718.266557] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 1
🚗 [BANH XE QUAY] Xung thu:   1 | Toc do mo phong:   1.5 km/h
[ 2719.366600] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 2
🚗 [BANH XE QUAY] Xung thu:   2 | Toc do mo phong:   3.0 km/h
[ 2720.656728] speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = 3
🚗 [BANH XE QUAY] Xung thu:   3 | Toc do mo phong:   4.5 km/h
```

---

## Bài 3: Tách Phần Cứng bằng Device Tree & Platform Driver (`step3_platform`)

### 1. Tại sao cần bài này?
* Ở Bài 2, số chân `540` bị gán cứng (`#define GPIO_SPEED_PIN 540`) trong file C. Nếu đổi sang chân khác ta lại phải sửa code C và biên dịch lại driver.
* Ở Bài 3, ta tách hoàn toàn phần cứng ra file riêng:
  * **File Device Tree (`speed_sensor.dts`):** Khai báo cảm biến cắm ở Bank 1 chân 28 (`<&gpio1 28 1>`) và đặt tên chuỗi nhận diện là `compatible = "car-ivi,speed-sensor";`.
  * **File Driver C (`speed_platform_driver.c`):** Đăng ký bảng `of_device_id`. Driver không cần biết số chân là bao nhiêu!
  * Khi Linux thấy phần cứng trong Device Tree khớp tên với Driver, Linux tự động gọi hàm **`probe()`**. Trong hàm `probe()`, ta dùng hàm `gpiod_get(dev, "speed", ...)` để Kernel tự đọc Device Tree và cấp phát chân GPIO + số ngắt IRQ tương ứng.

### 2. Các bước thực hiện chi tiết

#### Bước A: Viết file `speed_sensor.dts` và biên dịch sang `.dtbo` (Trên Ubuntu)
```dts
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            car_speed_sensor {
                compatible = "car-ivi,speed-sensor";
                speed-gpios = <&gpio1 28 1>; /* Bank 1, pin 28 (chân P9_12), Active-Low */
                status = "okay";
            };
        };
    };
};
```
Biên dịch ra file nhị phân:
```bash
~/bbb_workspace/buildroot/output/host/bin/dtc -O dtb -o speed_sensor.dtbo -b 0 -@ speed_sensor.dts
```

#### Bước B: Ghép file overlay vào cây thiết bị gốc của BeagleBone Black
Thay vì can thiệp vào U-Boot phức tạp, ta dùng công cụ `fdtoverlay` trên Ubuntu ghép trực tiếp vào file `.dtb` gốc:
```bash
# Lấy file DTB gốc ra
cp ~/bbb_workspace/buildroot/output/images/am335x-boneblack.dtb .

# Ghép nút cảm biến vào DTB gốc
~/bbb_workspace/buildroot/output/host/bin/fdtoverlay -i am335x-boneblack.dtb -o am335x-boneblack.dtb speed_sensor.dtbo
```

#### Bước C: Nạp file DTB mới vào phân vùng Boot và khởi động lại board
1. Trên BeagleBone Black:
   ```bash
   mkdir -p /mnt/boot && mount /dev/mmcblk0p1 /mnt/boot
   ```
2. Trên máy ảo Ubuntu:
   ```bash
   scp -O am335x-boneblack.dtb root@192.168.7.2:/mnt/boot/
   ```
3. Trên BeagleBone Black:
   ```bash
   sync && umount /mnt/boot && reboot
   ```

#### Bước D: Kiểm tra sau khi khởi động lại
Kiểm tra xem Kernel đã nhận nút phần cứng chưa:
```bash
ls -d /sys/firmware/devicetree/base/car_speed_sensor
# Kết quả in ra đúng đường dẫn trên là thành công!
```

Nạp module Platform Driver:
```bash
cd /root
insmod speed_platform_driver.ko
dmesg | tail -n 5
```

### 3. Kết quả chạy thực tế
```text
[   52.143670] speed_platform_driver: loading out-of-tree module taints kernel.
[   52.151995] car_speed_sensor car_speed_sensor: speed_platform: Bat tay phan cung thanh cong! Bat dau khoi tao...
[   52.165239] car_speed_sensor car_speed_sensor: Kernel tu cap phat IRQ so: 63 cho cam bien toc do
[   52.174845] car_speed_sensor car_speed_sensor: Platform Driver da san sang bat xung!
```
*Driver đã tự động ghép đôi (probe), tự nhận chân và ngắt IRQ 63 mà không cần gán cứng bất kỳ số chân nào trong code C!*

---

## Module Đồ Họa Framebuffer & Cảm Ứng Đa Điểm (`touch_gui`)

### 1. Mục đích
* Trực tiếp vẽ giao diện lên màn hình LCD qua cổng Micro-HDMI bằng cách ánh xạ bộ nhớ Framebuffer **`/dev/fb0`** qua hàm `mmap()`, sử dụng định dạng màu **RGB565**.
* Đọc tọa độ chạm ngón tay từ chip cảm ứng qua Linux Input Event **`/dev/input/event1`** (`hid-multitouch`).
* Xử lý va chạm tọa độ (Hit-Test):
  * Ở góc trên bên phải có nút **CLEAR** màu đỏ viền trắng. Khi ngón tay bấm trúng nút này, màn hình được xóa sạch.
  * Khi kéo ngón tay ở các vùng khác, màn hình vẽ nét cọ màu vàng bám theo đầu ngón tay.

### 2. Cách biên dịch và chạy
**Trên máy ảo Ubuntu:**
```bash
cd ~/car-ivi/touch_gui
make
make deploy
```

**Trên BeagleBone Black:**
```bash
cd /root
chmod +x touch_gui
./touch_gui
```

---

## Bài 4 (Đang phát triển): Giao tiếp Mạng CAN Bus (TWAI) giữa ESP32-C3 và BeagleBone Black

### 1. Sơ đồ phần cứng với 2 Module WCMCU-230 (chip TI SN65HVD230 3.3V)
* **Bên ESP32-C3 (Bộ điều khiển động cơ & chân ga):**
  * Biến trở chân ga: Chân giữa nối `GPIO 2` (ADC), 2 chân còn lại nối `3V3` và `GND`.
  * Module WCMCU-230 #1: Chân `CTX` nối `GPIO 5`, `CRX` nối `GPIO 6`, cấp nguồn `3V3` và `GND`.
* **Bên BeagleBone Black (Màn hình táp-lô & Gateway):**
  * Module WCMCU-230 #2: Chân `CTX` nối `P9_26` (DCAN1 TX), `CRX` nối `P9_24` (DCAN1 RX), cấp nguồn `P9_03` (3.3V) và `P9_01` (GND).
* **Đường truyền CAN giữa 2 module:**
  * Chân `CAN_H` của module 1 nối `CAN_H` của module 2.
  * Chân `CAN_L` của module 1 nối `CAN_L` của module 2.
  * Nối chung `GND` giữa 2 board.

### 2. Nguyên lý hoạt động
* ESP32-C3 dùng bộ điều khiển phần cứng TWAI (chuẩn CAN 2.0B) viết bằng **ESP-IDF**, đọc điện áp biến trở rồi đóng gói thành bản tin CAN ID `0x100` (8 bytes dữ liệu: chân ga, vận tốc, vòng tua RPM, cấp số...) bắn với tốc độ **125 kbps** với chu kỳ 50ms.
* BeagleBone Black dùng SocketCAN trong Linux (`can0`), nhận bản tin qua Socket C RAW và cập nhật trực tiếp lên giao diện màn hình.
