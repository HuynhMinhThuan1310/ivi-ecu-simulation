#!/usr/bin/env python3
"""
BeagleBone Black Automotive ECU and Telematics Gateway
Runs on BBB, interfaces with SocketCAN (vcan0/can0),
and streams telemetry to /dev/ttyGS0 for the Web-HMI.
"""

import os
import sys
import time
import json
import struct
import socket
import select
import threading

CAN_IFACE = 'vcan0'
SERIAL_DEV = '/dev/ttyGS0'

CAN_ID_POWERTRAIN = 0x100
CAN_ID_BATTERY    = 0x200
CAN_ID_BODY       = 0x300
CAN_ID_COMMAND    = 0x350

class CarState:
    def __init__(self):
        self.speed = 0.0          # km/h
        self.target_speed = 0.0
        self.rpm = 800
        self.gear = 'D'           # P, R, N, D
        self.throttle = 0
        self.soc = 88.0           # %
        self.range_km = 395
        self.temp_c = 29
        self.power_kw = 0.0
        self.odometer = 12450.2
        self.door_fl = False
        self.door_fr = False
        self.door_rl = False
        self.door_rr = False
        self.light_low = True
        self.light_high = False
        self.hazard = False
        self.turn_left = False
        self.turn_right = False
        self.locked = True
        self.drive_mode = 'COMFORT'  # ECO, COMFORT, SPORT
        self.raw_can_frames = []

state = CarState()
lock = threading.Lock()

def init_can():
    try:
        s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        s.bind((CAN_IFACE,))
        s.setblocking(False)
        print(f"[CAN] Successfully bound to {CAN_IFACE}")
        return s
    except Exception as e:
        print(f"[CAN Simulation] {CAN_IFACE} not active: {e}. Internal simulation running.")
        return None

def send_can_frame(can_sock, can_id, data):
    if not can_sock:
        return
    try:
        frame = struct.pack("=IB3x8s", can_id, len(data), data.ljust(8, b"\x00"))
        can_sock.send(frame)
    except Exception:
        pass

def physics_loop(can_sock):
    accel_dir = 1
    blink_timer = 0
    
    while True:
        with lock:
            if state.gear == 'D':
                max_speed = 130.0 if state.drive_mode == 'SPORT' else (80.0 if state.drive_mode == 'ECO' else 105.0)
                if accel_dir == 1:
                    state.target_speed = max_speed
                    if state.speed >= max_speed - 2:
                        accel_dir = -1
                else:
                    state.target_speed = 28.0
                    if state.speed <= 32.0:
                        accel_dir = 1
                
                rate = 1.0 if accel_dir == 1 else 1.6
                if state.drive_mode == 'SPORT': rate *= 1.4
                elif state.drive_mode == 'ECO': rate *= 0.7
                
                state.speed += (state.target_speed - state.speed) * 0.04 * rate
                state.throttle = int(max(0, min(100, (state.target_speed - state.speed) * 3)))
                
                state.rpm = int(800 + state.speed * 36.0)
                state.power_kw = round((state.speed / 120.0) * (62.0 if accel_dir == 1 else -22.0), 1)
            else:
                state.speed = 0.0
                state.throttle = 0
                state.rpm = 800
                state.power_kw = 0.0

            state.odometer += (state.speed / 3600.0) * 0.05
            if state.power_kw > 0:
                state.soc = max(5.0, state.soc - 0.003)
            else:
                state.soc = min(100.0, state.soc + 0.001)
            state.range_km = int(state.soc * 4.4)

            blink_timer += 1
            if blink_timer % 10 == 0:
                blink_state = bool((blink_timer // 10) % 2)
                if state.hazard:
                    state.turn_left = blink_state
                    state.turn_right = blink_state

            # CAN 0x100: Powertrain
            spd_raw = int(state.speed * 100)
            gear_byte = {'P': 0, 'R': 1, 'N': 2, 'D': 3}.get(state.gear, 0)
            data_100 = struct.pack(">HHBBxx", spd_raw, state.rpm, gear_byte, state.throttle)
            send_can_frame(can_sock, CAN_ID_POWERTRAIN, data_100)

            # CAN 0x200: Battery
            data_200 = struct.pack(">BBHhxx", int(state.soc), state.temp_c, state.range_km, int(state.power_kw * 10))
            send_can_frame(can_sock, CAN_ID_BATTERY, data_200)

            # CAN 0x300: Body
            d_byte = (int(state.door_fl) | (int(state.door_fr) << 1) | 
                      (int(state.door_rl) << 2) | (int(state.door_rr) << 3))
            l_byte = (int(state.light_low) | (int(state.light_high) << 1) | 
                      (int(state.hazard) << 2) | (int(state.turn_left) << 3) | 
                      (int(state.turn_right) << 4))
            mode_byte = {'ECO': 0, 'COMFORT': 1, 'SPORT': 2}.get(state.drive_mode, 1)
            data_300 = struct.pack("BBBB4x", d_byte, l_byte, int(state.locked), mode_byte)
            send_can_frame(can_sock, CAN_ID_BODY, data_300)

            state.raw_can_frames = [
                {"id": "0x100", "dlc": 8, "data": data_100.hex().upper(), "desc": f"Spd: {state.speed:.1f} km/h | RPM: {state.rpm} | Gear: {state.gear}"},
                {"id": "0x200", "dlc": 8, "data": data_200.hex().upper(), "desc": f"SoC: {int(state.soc)}% | Range: {state.range_km}km | Pwr: {state.power_kw}kW"},
                {"id": "0x300", "dlc": 8, "data": data_300.hex().upper(), "desc": f"Mode: {state.drive_mode} | Lock: {state.locked} | Doors: 0x{d_byte:02X}"}
            ]

        time.sleep(0.05)

def serial_writer_loop(serial_fd):
    while True:
        with lock:
            telemetry = {
                "speed": round(state.speed, 1),
                "rpm": state.rpm,
                "gear": state.gear,
                "throttle": state.throttle,
                "soc": int(state.soc),
                "range_km": state.range_km,
                "temp_c": state.temp_c,
                "power_kw": state.power_kw,
                "odometer": round(state.odometer, 1),
                "doors": {
                    "fl": state.door_fl,
                    "fr": state.door_fr,
                    "rl": state.door_rl,
                    "rr": state.door_rr
                },
                "lights": {
                    "low": state.light_low,
                    "high": state.light_high,
                    "hazard": state.hazard,
                    "left": state.turn_left,
                    "right": state.turn_right
                },
                "locked": state.locked,
                "drive_mode": state.drive_mode,
                "can_frames": state.raw_can_frames
            }
        line = json.dumps(telemetry) + "\n"
        try:
            os.write(serial_fd, line.encode('utf-8'))
        except Exception:
            pass
        time.sleep(0.04)

def serial_reader_loop(serial_fd, can_sock):
    buf = ""
    while True:
        try:
            r, _, _ = select.select([serial_fd], [], [], 0.5)
            if r:
                chunk = os.read(serial_fd, 256).decode('utf-8', errors='ignore')
                buf += chunk
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if line:
                        try:
                            cmd = json.loads(line)
                            handle_command(cmd, can_sock)
                        except json.JSONDecodeError:
                            pass
        except Exception:
            time.sleep(0.1)

def handle_command(cmd, can_sock):
    action = cmd.get("action")
    val = cmd.get("value")
    with lock:
        if action == "lock":
            state.locked = not state.locked
        elif action == "hazard":
            state.hazard = not state.hazard
            if not state.hazard:
                state.turn_left = False
                state.turn_right = False
        elif action == "headlight":
            state.light_low = not state.light_low
        elif action == "drive_mode":
            modes = ["ECO", "COMFORT", "SPORT"]
            idx = (modes.index(state.drive_mode) + 1) % len(modes)
            state.drive_mode = modes[idx]
        elif action == "gear":
            state.gear = val if val in ["P", "R", "N", "D"] else "D"
        elif action == "door":
            if val == "fl": state.door_fl = not state.door_fl
            elif val == "fr": state.door_fr = not state.door_fr
            elif val == "rl": state.door_rl = not state.door_rl
            elif val == "rr": state.door_rr = not state.door_rr

    cmd_bytes = json.dumps(cmd).encode('utf-8')[:8]
    send_can_frame(can_sock, CAN_ID_COMMAND, cmd_bytes)
    print(f"[ECU Command Executed]: {action} -> Broadcast CAN 0x350")

def main():
    print("=" * 60)
    print("  BEAGLEBONE BLACK AUTOMOTIVE ECU SIMULATOR & GATEWAY")
    print("=" * 60)
    
    can_sock = init_can()
    
    if not os.path.exists(SERIAL_DEV):
        print(f"[Error] {SERIAL_DEV} does not exist. Please run: sudo modprobe g_serial")
        sys.exit(1)
        
    try:
        os.system(f"chmod 666 {SERIAL_DEV}")
        serial_fd = os.open(SERIAL_DEV, os.O_RDWR | os.O_NOCTTY)
        print(f"[Serial] Opened {SERIAL_DEV} successfully.")
    except Exception as e:
        print(f"[Serial Error] Could not open {SERIAL_DEV}: {e}")
        sys.exit(1)

    t_phys = threading.Thread(target=physics_loop, args=(can_sock,), daemon=True)
    t_write = threading.Thread(target=serial_writer_loop, args=(serial_fd,), daemon=True)
    t_read = threading.Thread(target=serial_reader_loop, args=(serial_fd, can_sock), daemon=True)
    
    t_phys.start()
    t_write.start()
    t_read.start()
    
    print("[ECU] All engine threads active. Streaming vehicle telemetry to Windows...")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting ECU.")
        os.close(serial_fd)

if __name__ == '__main__':
    main()
