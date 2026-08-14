import serial
import time

# **** CRC16 TEST ****

PORT = "COM3"  # change to your ST-Link Virtual COM port (Device Manager -> Ports (COM & LPT))
BAUDRATE = 115200


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def build_frame(payload: bytes) -> bytes:
    crc = crc16_modbus(payload)
    crc_lo = crc & 0xFF
    crc_hi = (crc >> 8) & 0xFF
    return payload + bytes([crc_lo, crc_hi])


def main():
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    time.sleep(0.1)  # give the port a moment to open

    # Test 1: valid frame - should be echoed back
    payload = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x01])  # address=1, function=3, start=0, count=1
    frame = build_frame(payload)
    print(f"[1] Sending valid frame: {frame.hex(' ')}")
    ser.write(frame)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")
    print(f"    {'OK - echo matches' if response == frame else 'NO RESPONSE OR MISMATCH'}")

    time.sleep(0.5)

    # Test 2: corrupted frame (last CRC byte flipped) - should get no response
    bad_frame = frame[:-1] + bytes([frame[-1] ^ 0xFF])
    print(f"\n[2] Sending corrupted frame: {bad_frame.hex(' ')}")
    ser.write(bad_frame)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")
    print(f"    {'OK - correctly rejected' if not response else 'FAIL - responded despite bad CRC!'}")

    ser.close()


if __name__ == "__main__":
    main()
