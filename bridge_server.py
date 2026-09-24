"""
Automotive Telematics Serial-to-Web Bridge
Connects to BeagleBone Black over Serial (COM17/auto-detect),
serves Modern Digital Cockpit Web-HMI, and streams telemetry via WebSockets.
"""

import asyncio
import json
import logging
import os
import socket
import sys

# Ensure UTF-8 output on Windows consoles
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

from aiohttp import web
import serial
import serial.tools.list_ports

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("BridgeServer")

STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "public")
SERIAL_PORT = None
BAUD_RATE = 115200

ws_clients = set()
serial_conn = None

def find_bbb_port():
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        if any(tag in hwid.upper() for tag in ["0525", "A4A7", "A4A2", "GADGET"]):
            logger.info(f"--> Found BeagleBone Black USB Serial port: {port} ({desc})")
            return port
        if any(tag in desc.upper() or tag in hwid.upper() for tag in ["CP210", "CH340", "ESP", "USB-SERIAL"]):
            logger.info(f"--> Found ESP32 Serial port: {port} ({desc})")
            return port
    for p in ["COM17", "COM12"]:
        for port, desc, _ in ports:
            if port == p:
                return p
    return None

async def serial_reader_task():
    global serial_conn
    while True:
        port = SERIAL_PORT or find_bbb_port()
        if not port:
            logger.warning("ESP32 Serial port not detected yet. Retrying in 2s...")
            await asyncio.sleep(2)
            continue

        try:
            logger.info(f"Connecting to ESP32 on {port}...")
            serial_conn = serial.Serial(port, BAUD_RATE, timeout=0.05)
            logger.info(f"Connected to ESP32 on {port} successfully!")

            buffer = ""
            while True:
                if serial_conn.in_waiting > 0:
                    raw_data = serial_conn.read(serial_conn.in_waiting).decode("utf-8", errors="ignore")
                    buffer += raw_data
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        line = line.strip()
                        if line.startswith("{") and line.endswith("}"):
                            dead = set()
                            for ws in ws_clients:
                                try:
                                    await ws.send_str(line)
                                except Exception:
                                    dead.add(ws)
                            ws_clients.difference_update(dead)
                await asyncio.sleep(0.01)

        except serial.SerialException as e:
            logger.error(f"Serial connection error: {e}. Retrying in 2s...")
            if serial_conn and serial_conn.is_open:
                serial_conn.close()
            serial_conn = None
            await asyncio.sleep(2)
        except Exception as e:
            logger.error(f"Reader error: {e}")
            await asyncio.sleep(1)

async def websocket_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    ws_clients.add(ws)
    logger.info(f"Cockpit HMI connected. Active screens: {len(ws_clients)}")

    try:
        async for msg in ws:
            if msg.type == web.WSMsgType.TEXT:
                cmd_str = msg.data.strip() + "\n"
                logger.info(f"[HMI Command]: {msg.data}")
                if serial_conn and serial_conn.is_open:
                    serial_conn.write(cmd_str.encode("utf-8"))
                    serial_conn.flush()
    finally:
        ws_clients.remove(ws)
        logger.info(f"Cockpit HMI disconnected. Active screens: {len(ws_clients)}")

    return ws

async def index_handler(request):
    return web.FileResponse(os.path.join(STATIC_DIR, "index.html"))

def create_app():
    app = web.Application()
    app.router.add_get("/", index_handler)
    app.router.add_get("/ws", websocket_handler)
    app.router.add_static("/", STATIC_DIR, show_index=True)
    return app

async def main():
    app = create_app()
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", 8080)
    await site.start()
    
    local_ip = get_local_ip()
    print("=" * 65)
    print("🚗 MODERN DIGITAL COCKPIT WEB-HMI READY!")
    print(f"👉 Máy tính mở trình duyệt:  http://localhost:8080")
    print(f"📱 Điện thoại cùng Wi-Fi mở: http://{local_ip}:8080")
    print("⚠️  LƯU Ý QUAN TRỌNG: Dùng 'http://' (KHÔNG dùng 'https://')")
    print("=" * 65)

    asyncio.create_task(serial_reader_task())

    while True:
        await asyncio.sleep(3600)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        SERIAL_PORT = sys.argv[1]
    asyncio.run(main())
