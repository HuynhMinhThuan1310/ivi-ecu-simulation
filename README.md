# 🚗 SMART CAR-IVI OS, TELEMATICS GATEWAY & CAN-FOTA AUTOMOTIVE

Dự án mô phỏng hệ sinh thái **In-Vehicle Infotainment (Car-IVI)**, **Central Telematics Gateway** và **Nâng cấp Firmware từ xa qua mạng CAN (CAN-FOTA)** hoàn chỉnh theo tiêu chuẩn công nghiệp Automotive.

---

## 🏛️ 1. Kiến trúc Tổng thể Hệ thống (System Architecture)

```
[ ESP32 + Biến trở ] 
   (Đọc ADC chân ga 0 - 100%, quy đổi vận tốc 0 - 200 km/h)
          │
          ├── [Đường Serial/COM20] ──> [ Python esp_to_ivi.py ] ──> [ Node.js HTTPS Server (Port 8080) ]
          │                                                               │
          │                                                               ├──> 🗺️ Bản đồ Live GPS (Leaflet + OSRM)
          │                                                               └──> 🏎️ Táp-lô Mercedes (Speedometer Canvas)
          │
          └── [Đường Bus CAN - 125kbps] ── (SN65HVD230) ──> [ BeagleBone Black (Central Gateway ECU) ]
                                                                   │
                                                                   ├──> SocketCAN (can0)
                                                                   ├──> car_ecu.py (Xử lý dữ liệu xe)
                                                                   ├──> Xuất hình Micro-HDMI ra Màn hình LCD Cảm ứng
                                                                   └──> 🚀 CAN-FOTA (Nâng cấp Firmware qua CAN)
```

---

## 🖥️ 2. Hệ thống Giao diện Mercedes Car-IVI (Web HMI)

### A. Backend: `server.js` (Node.js HTTPS + Socket.IO)
* **Cổng dịch vụ:** `8080` (HTTPS bảo mật SSL với `key.pem` và `cert.pem`).
* **API POST `/speed`:** Tiếp nhận vận tốc từ ESP32 biến trở, phát sự kiện broadcast `speed_update` tới tất cả các màn hình táp-lô theo thời gian thực.
* **Socket.IO `gps_update`:** Phân phối tọa độ vĩ độ/kinh độ và góc xoay hướng xe (`heading`).
* **Điều khiển Actuators (`actuator_cmd`):** Tiếp nhận thao tác từ UI (Đóng/Mở Nắp Capo, Cửa xe, Cốp sau) để chuyển tiếp xuống mạng CAN.

### B. Xử lý Bản đồ & Di chuyển Thực tế: `app.js`
* **Bản đồ Leaflet OpenStreetMap:** Tâm ban đầu tại Đà Nẵng (`[16.0738, 108.1499]`).
* **Mô phỏng Xe Sedan SVG siêu thực:** Kích thước $26 \times 52\text{ px}$, phủ gradient metallic, hiệu ứng bóng đổ tự nhiên (`realCarShadow`), đèn pha LED và đèn hậu đỏ.
* **Định tuyến OSRM (`router.project-osrm.org`):** Tự động vẽ lộ trình ngắn nhất khi người dùng chọn điểm đến, hiển thị chính xác cự ly (km) và thời gian (phút).
* **Thuật toán Vật lý Xe Di chuyển (`carPhysicsLoop`):**
  * Đổi vận tốc từ $\text{km/h}$ (biến trở) sang $\text{m/s}$ ($v_{\text{ms}} = v_{\text{km/h}} \times \frac{1000}{3600}$).
  * Tính toán góc nghiêng tim đường (`getBearing`) để xoay đầu xe theo góc cua khúc đường và điều khiển camera bám sát xe.

### C. Giao diện Táp-lô Buồng lái: `index.html` (Mercedes IVI OS)
* **Top Bar:** Đồng hồ thời gian thực, trạng thái xe (`⚡ SYSTEM READY - P`).
* **Sidebar chuyển Tab:**
  * **Tab `🚗 XE` (Vehicle Actuators):** Các đốm sáng tương tác (Hotspots) điều khiển Nắp Capo (`pin-hood`), Cửa xe (`pin-door`), Cốp sau (`pin-trunk`).
  * **Tab `🏎 SPEED` (Drive Dashboard):**
    * **Speedometer Canvas 270°:** Dải màu Gradient Cyan $\rightarrow$ Sky Blue $\rightarrow$ Red (khi vượt quá 180 km/h).
    * **RPM Gauge Canvas 8000 RPM:** Tự động tính số D (1 đến 6 theo tốc độ).
    * Thẻ hiển thị mức nhiên liệu (78%), nhiệt độ động cơ (87°C).
* **HUD Panel:** Tìm kiếm địa điểm (Nominatim Geocoding), nút Bắt đầu, Tạm dừng và Hủy lộ trình.

---

## 🧠 3. Phần cứng BeagleBone Black (Central Gateway & Display)

* **Bộ xử lý:** TI AM3358 ARM Cortex-A8, chạy **Embedded Linux Buildroot 2025.02** siêu nhẹ.
* **Phương thức cấp nguồn & điều khiển chuyên nghiệp:**
  * **Nguồn:** Jack tròn DC 5V/2A ổn định, nuôi độc lập board và màn hình LCD cảm ứng.
  * **Debug & Điều khiển:** Cáp UART Serial **COM8** (`QinHeng USB Serial CH340` - Baudrate `115200`).
* **Màn hình hiển thị & Cảm ứng:**
  * Cáp **Micro-HDMI** kết nối với chip phát **NXP TDA19988** trên board, kích hoạt driver DRM `/dev/dri/card0` xuất tín hiệu ra LCD.
  * Cáp USB Cảm ứng nhận diện tự động qua driver **`hid-multitouch`** (chip cảm ứng `wch.cn USB2IIC_CTP_CONTROL`).
* **Môi trường chạy:**
  * Toolchain: Bootlin External Toolchain (`armv7-eabihf glibc stable`).
  * Tích hợp: `Node.js`, `NPM`, `Python3`, `can-utils`, `libgpiod`.

---

## 🔌 4. Sơ đồ Đấu nối Bus CAN Chuẩn Tầng Vật lý

### A. BeagleBone Black $\longleftrightarrow$ Module WCMCU-230 #1
| Chân BeagleBone Black (P9) | Chân Module WCMCU-230 | Chức năng |
|---|:---:|---|
| **P9_03** | **3V3** | Nguồn 3.3V |
| **P9_01** | **GND** | Mass |
| **P9_24** | **CRX** | DCAN1_RX |
| **P9_26** | **CTX** | DCAN1_TX |

### B. ESP32 $\longleftrightarrow$ Module WCMCU-230 #2
| Chân ESP32 | Chân Module WCMCU-230 | Chức năng |
|---|:---:|---|
| **3V3** | **3V3** | Nguồn 3.3V |
| **GND** | **GND** | Mass |
| **GPIO 4** | **CRX** | TWAI RX |
| **GPIO 5** | **CTX** | TWAI TX |

### C. Đường Bus CAN giữa 2 Module
* `CAN_H` nối `CAN_H`, `CAN_L` nối `CAN_L`, `GND` chung.
* **Điện trở đầu cuối:** 2 điện trở $120\ \Omega$ kẹp tại 2 đầu bus (tổng trở toàn bus đo được $\approx 60\ \Omega$ chuẩn ISO 11898).

---

## 🚀 5. Giao thức Nâng cấp Firmware từ xa (CAN-FOTA)

Master Linux (BeagleBone Black) nạp từ xa firmware cho ESP32 qua bus CAN 125 kbps:

1. **Lớp vận chuyển (Transport Layer):** Chuẩn **ISO-TP (ISO 15765-2)** chia nhỏ file nhị phân thành các khung CAN 8-byte (Single Frame, First Frame, Consecutive Frame, Flow Control).
2. **Lớp ứng dụng (Application Layer - UDS style):**
   * **0x10 / 0x34 Request Download:** Bắt tay kiểm tra ID phần mềm.
   * **0x31 Erase Flash:** Xóa phân vùng OTA nhàn rỗi (`ota_0` hoặc `ota_1`) trên ESP32.
   * **0x36 Transfer Data:** Truyền từng khối block nhị phân (kèm offset).
   * **0x37 Request Transfer Exit:** Xác thực mã băm kiểm tra toàn vẹn CRC32.
   * **0x11 ECU Reset:** Đổi boot flag phân vùng và khởi động lại ESP32 vào firmware mới.

---

## 📦 6. Cấu trúc Thư mục Kho lưu trữ Git Chuẩn (GitHub Repository Structure)

Để gom toàn bộ các file rải rác ở Windows, máy ảo Ubuntu và ESP32 vào 1 kho Git chuyên nghiệp:

```
smart-car-ivi-gateway/
├── docs/                        # Tài liệu hệ thống, sơ đồ phần cứng, hình ảnh demo
│   ├── ARCHITECTURE.md
│   └── PINOUT.md
├── web-ivi/                     # Giao diện Web HMI (Bản đồ Leaflet & Speedometer)
│   ├── server.js                # Backend HTTPS + Socket.IO (Port 8080)
│   ├── map-module/              # Trang bản đồ định vị GPS
│   │   ├── index.html
│   │   ├── app.js
│   │   └── certs/               # File chứng chỉ SSL (key.pem, cert.pem)
│   └── speedometer/             # Táp-lô đồng hồ điện tử
│       ├── index.html
│       └── app.js
├── bbb-gateway/                 # Phần mềm chạy trên BeagleBone Black
│   ├── car_ecu.py               # Telematics Gateway Daemon
│   ├── can_bridge.py            # Cầu nối SocketCAN <-> UDP
│   ├── init.d/                  # Scripts khởi động tự động (/etc/init.d/)
│   │   └── S20usbnet
│   └── buildroot/               # Cấu hình Buildroot
│       └── bbb_custom_defconfig # File cấu hình OS rút gọn
├── esp32-nodes/                 # Mã nguồn các board ESP32
│   ├── esp32-bientro-car/       # Node đọc biến trở ADC & xuất telemetry
│   │   └── main/bientro.c
│   └── esp32-can-fota/          # Node nhận CAN 0x350 điều khiển LED & nạp FOTA
│       └── main/fota_client.c
├── tools/                       # Các công cụ hỗ trợ trên máy tính Windows
│   ├── esp_to_ivi.py            # Script đọc cổng COM đẩy vào Web
│   └── run_proxy.bat            # Script chuyển tiếp cổng mạng
├── .gitignore                   # Loại trừ các file rác build, .bin, node_modules
└── README.md                    # Trang giới thiệu dự án chính trên GitHub
```
