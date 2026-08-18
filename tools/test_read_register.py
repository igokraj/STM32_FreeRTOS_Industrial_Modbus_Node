import serial
import time

# **** MODBUS READ HOLDING REGISTERS (0x03) TEST ****

PORT = "COM3"  # change to your ST-Link Virtual COM port (Device Manager -> Ports (COM & LPT))
BAUDRATE = 115200

SLAVE_ADDRESS = 1
READ_HOLDING_REGISTERS = 0x03

SENSOR_ERROR_VALUE = 0xFFFF  # returned by read_htu21d_temperature() when I2C fails
PLAUSIBLE_TEMP_MAX = 600     # 60.0 C - anything above this means something is wrong

RESET_CAUSE_NAMES = {
    0: "UNKNOWN",
    1: "POWER_ON",
    2: "PIN",
    3: "WATCHDOG_IWDG",
    4: "WATCHDOG_WWDG",
    5: "SOFTWARE",
    6: "BROWNOUT",
}


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

    # Test 1: read register 0 - temperature from the HTU21D, scaled x10 (e.g. 253 = 25.3 C)
    request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=0, reg_count=1)
    print(f"[1] Sending request (read register 0 - temperature): {request.hex(' ')}")
    ser.write(request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")

    if len(response) == 7:
        raw = (response[3] << 8) | response[4]
        if raw == SENSOR_ERROR_VALUE:
            print(f"    Temperature: {raw} -> SENSOR FAULT (I2C read failed)")
        elif 0 < raw <= PLAUSIBLE_TEMP_MAX:
            print(f"    Temperature: {raw / 10:.1f} C")
            print("    OK - value in a plausible range")
        else:
            print(f"    Temperature: {raw / 10:.1f} C")
            print("    FAIL - value outside the plausible range")
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

    # Test 3: read register 2 - cause of the last reset (logged to Flash at boot)
    reset_cause_request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=2, reg_count=1)
    print(f"\n[3] Sending request (read register 2 - last reset cause): {reset_cause_request.hex(' ')}")
    ser.write(reset_cause_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")

    if len(response) == 7:
        reset_cause = (response[3] << 8) | response[4]
        name = RESET_CAUSE_NAMES.get(reset_cause, "UNKNOWN VALUE")
        print(f"    Last reset cause: {reset_cause} ({name})")
    else:
        print("    FAIL - unexpected response length")

    time.sleep(0.5)

    # Test 4: read register 3 - firmware version (high byte = major, low byte = minor)
    version_request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=3, reg_count=1)
    print(f"\n[4] Sending request (read register 3 - firmware version): {version_request.hex(' ')}")
    ser.write(version_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")

    if len(response) == 7:
        raw = (response[3] << 8) | response[4]
        major = (raw >> 8) & 0xFF
        minor = raw & 0xFF
        print(f"    Firmware version: v{major}.{minor}  (raw 0x{raw:04X})")
        print(f"    {'OK - FreeRTOS build is running' if raw >= 0x0200 else 'WARNING - looks like the old bare-metal firmware'}")
    else:
        print("    FAIL - unexpected response length")

    time.sleep(0.5)

    # Test 5: read register 4 - number of resets appended to the Flash log
    count_request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=4, reg_count=1)
    print(f"\n[5] Sending request (read register 4 - reset count): {count_request.hex(' ')}")
    ser.write(count_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")

    if len(response) == 7:
        count = (response[3] << 8) | response[4]
        print(f"    Resets logged: {count}")
        print("    (run this again after a reset - the number should grow by one)")
    else:
        print("    FAIL - unexpected response length")

    time.sleep(0.5)

    # Test 6: read out-of-range register (6, but REGISTER_COUNT=5) - should get no response
    bad_request = build_read_request(SLAVE_ADDRESS, READ_HOLDING_REGISTERS, reg_address=6, reg_count=1)
    print(f"\n[6] Sending request (out-of-range register 6): {bad_request.hex(' ')}")
    ser.write(bad_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")
    print(f"    {'OK - correctly ignored' if not response else 'FAIL - responded to out-of-range register'}")

    time.sleep(0.5)

    # Test 7: wrong slave address (2 instead of 1) - should get no response
    wrong_addr_request = build_read_request(2, READ_HOLDING_REGISTERS, reg_address=0, reg_count=1)
    print(f"\n[7] Sending request (wrong slave address 2): {wrong_addr_request.hex(' ')}")
    ser.write(wrong_addr_request)
    time.sleep(0.1)
    response = ser.read(64)
    print(f"    Received: {response.hex(' ') if response else '(nothing)'}")
    print(f"    {'OK - correctly ignored' if not response else 'FAIL - responded to wrong address'}")

    ser.close()


if __name__ == "__main__":
    main()
