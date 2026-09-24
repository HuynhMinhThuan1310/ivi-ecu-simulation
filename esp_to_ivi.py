"""
ESP32 Potentiometer to Car-IVI Bridge
Đọc giá trị biến trở từ ESP32 qua cổng Serial (COM20)
và gửi POST request vào IVI HTTPS Server trên Ubuntu VM (192.168.18.130:8080/speed).
"""

import sys
import time
import json
import ssl
import http.client
import serial
import serial.tools.list_ports

TARGET_HOST = "192.168.18.130"
TARGET_PORT = 8080
DEFAULT_PORT = "COM20"
BAUD_RATE = 115200

def find_esp_port():
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        if any(tag in desc.upper() or tag in hwid.upper() for tag in ["CP210", "CH340", "ESP", "USB-SERIAL"]):
            return port
    for port, desc, _ in ports:
        if port == DEFAULT_PORT:
            return DEFAULT_PORT
    return DEFAULT_PORT

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_esp_port()
    
    print("=" * 60)
    print("🚀 ESP32 -> CAR-IVI BRIDGE (UBUNTU VM)")
    print(f"📡 Cổng ESP32:     {port}")
    print(f"🎯 Đích chuyển tiếp: https://{TARGET_HOST}:{TARGET_PORT}/speed")
    print("=" * 60)

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
        print(f"✅ Đã kết nối thành công với ESP32 trên {port}!")
    except Exception as e:
        print(f"❌ Không thể mở cổng {port}: {e}")
        print("💡 Lưu ý: Kiểm tra xem có terminal nào khác đang giữ cổng COM không.")
        return

    # Khởi tạo SSL context bỏ qua chứng chỉ tự ký của IVI server
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    conn = None
    last_speed = -1
    last_send_time = 0

    print("⚡ Đang lắng nghe biến trở và truyền vào Speedometer...")
    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("{") and line.endswith("}"):
                try:
                    telemetry = json.loads(line)
                except json.JSONDecodeError:
                    continue

                speed = telemetry.get("speed", 0)
                now = time.time()

                # Gửi khi tốc độ thay đổi hoặc tối thiểu mỗi 100ms một lần
                if speed != last_speed or (now - last_send_time) >= 0.1:
                    last_speed = speed
                    last_send_time = now

                    # Sử dụng HTTP keep-alive connection để truyền cực nhanh
                    if conn is None:
                        conn = http.client.HTTPSConnection(TARGET_HOST, TARGET_PORT, context=ctx, timeout=1.0)

                    payload = json.dumps({"speed": speed})
                    headers = {"Content-Type": "application/json"}
                    
                    try:
                        conn.request("POST", "/speed", body=payload, headers=headers)
                        res = conn.getresponse()
                        res.read()  # đọc hết response để tái sử dụng connection
                        print(f"🏎  [ESP32] Tốc độ: {speed:3d} km/h  -->  Đã gửi vào IVI Server")
                    except Exception:
                        # Kết nối lại nếu đứt
                        if conn:
                            conn.close()
                        conn = None

        except serial.SerialException as e:
            print(f"⚠️ Mất kết nối Serial: {e}. Đang thử lại sau 2s...")
            time.sleep(2)
        except KeyboardInterrupt:
            print("\nĐã dừng cầu nối.")
            break
        except Exception as e:
            time.sleep(0.05)

    if ser and ser.is_open:
        ser.close()
    if conn:
        conn.close()

if __name__ == "__main__":
    main()
