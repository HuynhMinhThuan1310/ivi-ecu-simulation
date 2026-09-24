# HƯỚNG DẪN VẬN HÀNH HỆ THỐNG CAR-IVI VÀ BIẾN TRỞ ESP32

> **Tài liệu tổng hợp câu lệnh và các bước vận hành từ A - Z**

---

## 1. TỔNG QUAN KIẾN TRÚC HOẠT ĐỘNG

- **ESP32 + Biến trở:** Đọc giá trị góc xoay biến trở chân ADC, tính ra giá trị % chân ga và tốc độ (0 - 200 km/h), gửi chuỗi JSON qua cổng cáp USB Serial (**COM20**).
- **Cầu nối Python (`esp_to_ivi.py` trên Windows):** Mở cổng COM20, đọc tốc độ và gửi HTTP POST trực tiếp sang máy ảo Ubuntu (`https://192.168.18.130:8080/speed`).
- **Backend IVI (`server.js` trên Ubuntu VM):** Nhận tốc độ qua API `/speed`, phát qua Socket.IO để cập nhật đồng hồ tốc độ và bản đồ định vị theo thời gian thực.
- **Cầu nối mạng (`run_proxy.bat` trên Windows):** Chuyển tiếp cổng 8080 từ Windows vào máy ảo Ubuntu để trình duyệt và điện thoại truy cập được.

---

## 2. TRÌNH TỰ KHỞI ĐỘNG HỆ THỐNG (MỖI LẦN CHẠY)

Mỗi lần chạy hệ thống, bạn mở **3 cửa sổ dòng lệnh** theo thứ tự sau:

| Cửa sổ | Môi trường | Lệnh cần gõ | Giải thích |
| :--- | :--- | :--- | :--- |
| **Cửa sổ 1** | **Ubuntu VM** *(MobaXterm)* | `cd ~/car-ivi`<br>`node server.js` | Khởi động Backend IVI (Bản đồ + Speedometer). |
| **Cửa sổ 2** | **Windows CMD** | `cd C:\prj-bbb`<br>`run_proxy.bat` | Khởi động cầu nối chuyển tiếp mạng cổng 8080 vào máy ảo. *(hoặc nhấp đúp file run_proxy.bat)* |
| **Cửa sổ 3** | **Windows Terminal** | `cd C:\prj-bbb`<br>`python esp_to_ivi.py` | Đọc biến trở ESP32 (COM20) và đẩy tốc độ vào Ubuntu. |

---

## 3. ĐỊA CHỈ TRUY CẬP TRÊN TRÌNH DUYỆT (CHROME / EDGE / SAFARI)

Sau khi cả 3 cửa sổ trên đã chạy:

### 👉 Trên máy tính:
- **Đồng hồ tốc độ (Speedometer):**  
  `https://localhost:8080/speedometer/`
- **Bản đồ định vị GPS:**  
  `https://localhost:8080/`

### 📱 Trên điện thoại (kết nối cùng mạng Wi-Fi với máy tính):
- **Đồng hồ tốc độ (Speedometer):**  
  `https://10.0.14.56:8080/speedometer/`
- **Bản đồ định vị GPS:**  
  `https://10.0.14.56:8080/`

> **⚠️ LƯU Ý KHI TRUY CẬP LẦN ĐẦU:**  
> Trình duyệt sẽ hiện cảnh báo bảo mật màu đỏ *"Kết nối của bạn không phải là kết nối riêng tư"* do chứng chỉ SSL tự tạo.  
> 👉 Bạn chỉ cần bấm: **Nâng cao (Advanced)** → **Tiếp tục truy cập (Proceed to localhost)**.

---

## 4. LỆNH NẠP CODE VÀO ESP32 (KHI CẦN SỬA CODE C)

Nếu bạn chỉnh sửa file `bientro.c`:

1. **Tắt Cửa sổ 3 (`esp_to_ivi.py`)** bằng tổ hợp phím `Ctrl + C` (để nhả cổng COM20).
2. Mở Terminal tại thư mục `C:\prj-bbb\bientro\hello_world` và gõ:
   ```powershell
   python C:\Espressif\frameworks\esp-idf-v5.4.2\tools\idf.py build flash -p COM20
   ```
3. Nạp xong 100%, bật lại Cửa sổ 3:
   ```powershell
   python esp_to_ivi.py
   ```

---

## 5. BẢNG XỬ LÝ NHANH CÁC LỖI THƯỜNG GẶP

| Lỗi gặp phải | Nguyên nhân | Cách khắc phục |
| :--- | :--- | :--- |
| `[LỖI] Cổng 8080 đang có ứng dụng khác sử dụng!` | Do đang bật `bridge_server.py` cũ chiếm mất cổng 8080. | Tắt cửa sổ `bridge_server.py`. Chỉ dùng `run_proxy.bat` và `esp_to_ivi.py`. |
| `PermissionError: 'Access is denied' trên cổng COM20` | Do Serial Monitor của VS Code hoặc terminal khác đang giữ cổng COM20. | Đóng tab Serial Monitor trong VS Code hoặc rút cáp USB ESP32 cắm lại. |
| `SyntaxError: Invalid or unexpected token (Node.js...)` khi gõ `idf.py` | Do Windows gán nhầm file đuôi `.py` cho Node.js. | Thêm `python` ở đầu lệnh: `python ...\idf.py build flash -p COM20`. |
