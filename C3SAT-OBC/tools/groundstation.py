#!/usr/bin/env python3
"""Minimal C3SAT-OBC ground station.

Speaks the same CCSDS-inspired frame format as components/services/telecommand.c.
Sends telecommands over the UART1 "radio" link and decodes downlinked beacons.

Wiring: connect a USB-UART adapter to the ESP32-C6 UART1 pins
(BSP_PIN_UART_TX=GPIO16 -> adapter RX, BSP_PIN_UART_RX=GPIO17 -> adapter TX, GND-GND).

Examples:
    python groundstation.py --port /dev/ttyUSB0 ping
    python groundstation.py --port /dev/ttyUSB0 set-mode NOMINAL
    python groundstation.py --port /dev/ttyUSB0 listen
"""
import argparse
import struct
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("Please 'pip install pyserial' first.")

SYNC0, SYNC1 = 0xC3, 0x5A           # uplink markers (downlink swaps them)
TM_SYNC0, TM_SYNC1 = SYNC1, SYNC0

APID = {"CDH": 0, "EPS": 1, "ADCS": 2, "THERMAL": 3, "PAYLOAD": 4}
MODES = {"BOOT": 0, "SAFE": 1, "NOMINAL": 2, "PAYLOAD": 3, "FAULT": 4}
MODE_NAMES = {v: k for k, v in MODES.items()}
OP = {
    "nop": 0x00, "ping": 0x01, "set-mode": 0x02, "beacon": 0x03,
    "clear-faults": 0x04, "heater": 0x05, "dump-log": 0x06, "reboot": 0x07,
}


def crc16(data: bytes) -> int:
    """CRC16/CCITT-FALSE (poly 0x1021, init 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(apid: int, opcode: int, seq: int, payload: bytes) -> bytes:
    hdr = bytes([apid & 0xFF, opcode & 0xFF, seq & 0xFF, len(payload)])
    body = hdr + payload
    crc = crc16(body)
    return bytes([SYNC0, SYNC1]) + body + struct.pack(">H", crc)


def decode_beacon(payload: bytes):
    """Best-effort unpack of obc_beacon_t (see obc_types.h). Layout is
    compiler-dependent; treat field offsets as indicative."""
    if len(payload) < 16:
        return None
    uptime, mission, boots, mode = struct.unpack_from("<IIIi", payload, 0)
    return {
        "uptime_s": uptime,
        "mission_s": mission,
        "boots": boots,
        "mode": MODE_NAMES.get(mode, f"?{mode}"),
    }


def listen(ser, duration=None):
    buf = bytearray()
    start = time.time()
    while duration is None or time.time() - start < duration:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            # hunt for a downlink frame
            while len(buf) >= 6:
                if buf[0] != TM_SYNC0 or buf[1] != TM_SYNC1:
                    buf.pop(0)
                    continue
                length = buf[3]
                total = 4 + length + 2
                if len(buf) < total:
                    break
                frame = bytes(buf[:total])
                del buf[:total]
                want = struct.unpack(">H", frame[-2:])[0]
                got = crc16(frame[2:-2])
                tag = "OK " if want == got else "BAD"
                b = decode_beacon(frame[4:4 + length])
                print(f"[{tag}] TM len={length} {b}")
        else:
            time.sleep(0.02)


def main():
    ap = argparse.ArgumentParser(description="C3SAT-OBC ground station")
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ping")
    sub.add_parser("beacon")
    sub.add_parser("clear-faults")
    sub.add_parser("dump-log")
    sub.add_parser("reboot")
    p_mode = sub.add_parser("set-mode")
    p_mode.add_argument("mode", choices=list(MODES))
    p_heat = sub.add_parser("heater")
    p_heat.add_argument("state", choices=["on", "off", "auto"])
    p_listen = sub.add_parser("listen")
    p_listen.add_argument("--seconds", type=float, default=None)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)

    if args.cmd == "listen":
        listen(ser, args.seconds)
        return

    payload = b""
    if args.cmd == "set-mode":
        payload = bytes([MODES[args.mode]])
    elif args.cmd == "heater":
        payload = bytes([{"off": 0, "on": 1, "auto": 0xFF}[args.state]])

    frame = encode(APID["CDH"], OP[args.cmd], seq=1, payload=payload)
    ser.write(frame)
    print(f"TX {args.cmd}: {frame.hex(' ')}")
    listen(ser, duration=2.0)  # show any reply/beacon for 2 s


if __name__ == "__main__":
    main()
