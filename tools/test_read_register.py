import serial
import time

# **** MODBUS READ HOLDING REGISTERS (0x03) TEST ****

PORT = "COM3"  # change to your ST-Link Virtual COM port (Device Manager -> Ports (COM & LPT))
BAUDRATE = 115200

SLAVE_ADDRESS = 1
READ_HOLDING_REGISTERS = 0x03


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


def build_read_request(slave_address: int, function_code: int, reg_address: int, reg_count: int) -> bytes:
    payload = bytes([
        slave_address,
        function_code,
        (reg_address >> 8) & 0xFF,
        reg_address & 0xFF,
        (reg_count >> 8) & 0xFF,
        reg_count & 0xFF,
    ])
    return build_frame(payload)


def main():
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    time.sleep(0.1)  # give the port a moment to open

    # Test 1: read register 0 - expected value 1234 (set in holding_registers_map in main.c)
    request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=0, reg_count=1)
    print(f"[1] Sending request (read register 0): {request.hex(' ')}")
    ser.write(request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")

    if len(response) == 7:
        value = (response[3] << 8) | response[4]
        expected = 1234
        print(f"    Register value: {value} (expected {expected})")
        print(f"    {'OK - value matches' if value == expected else 'FAIL - value mismatch'}")
    else:
        print("    FAIL - unexpected response length")

    time.sleep(0.5)

    # Test 2: read register 1 - cylinder limit switch state (raw 0/1, no scaling)
    switch_request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=1, reg_count=1)
    print(f"\n[2] Sending request (read register 1 - limit switch): {switch_request.hex(' ')}")
    ser.write(switch_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")

    if len(response) == 7:
        switch_state = (response[3] << 8) | response[4]
        print(f"    Limit switch state: {switch_state} ({'pressed' if switch_state == 1 else 'released' if switch_state == 0 else 'unexpected value'})")
    else:
        print("    FAIL - unexpected response length")

    time.sleep(0.5)

    # Test 3: read out-of-range register (5, but REGISTER_COUNT=4) - should get no response
    bad_request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=5, reg_count=1)
    print(f"\n[3] Sending request (out-of-range register 5): {bad_request.hex(' ')}")
    ser.write(bad_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")
    print(f"    {'OK - correctly ignored' if not response else 'FAIL - responded to out-of-range register'}")

    time.sleep(0.5)

    # Test 3: wrong slave address (2 instead of 1) - should get no response
    wrong_addr_request = build_read_request(2, READ_HOLDING_REGISTERS, reg_address=0, reg_count=1)
    print(f"\n[4] Sending request (wrong slave address 2): {wrong_addr_request.hex(' ')}")
    ser.write(wrong_addr_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")
    print(f"    {'OK - correctly ignored' if not response else 'FAIL - responded to wrong address'}")

    ser.close()


if __name__ == "__main__":
    main()
