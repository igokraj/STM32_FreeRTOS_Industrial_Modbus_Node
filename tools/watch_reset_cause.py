import serial
import time

# **** RESET CAUSE WATCHER ****
#
# Polls register 2 (last reset cause) over and over for a fixed period.
# Useful when the device keeps resetting: single test runs can easily fall into
# the dead window while the MCU is rebooting, so we just keep asking.

PORT = "COM3"  # change to your ST-Link Virtual COM port (Device Manager -> Ports (COM & LPT))
BAUDRATE = 115200

SLAVE_ADDRESS = 1
READ_HOLDING_REGISTERS = 0x03

WATCH_SECONDS = 20
POLL_INTERVAL = 0.2

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


def build_read_request(reg_address: int) -> bytes:
    payload = bytes([
        SLAVE_ADDRESS,
        READ_HOLDING_REGISTERS,
        (reg_address >> 8) & 0xFF,
        reg_address & 0xFF,
        0x00,
        0x01,
    ])
    crc = crc16_modbus(payload)
    return payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def main():
    ser = serial.Serial(PORT, BAUDRATE, timeout=0.3)
    time.sleep(0.1)

    reset_request = build_read_request(2)
    version_request = build_read_request(3)

    print(f"Polling register 2 (reset cause) for {WATCH_SECONDS} s...")
    print("(dots = no answer, the device is most likely rebooting)")
    print()

    seen = {}
    version_seen = None
    deadline = time.time() + WATCH_SECONDS

    while time.time() < deadline:
        ser.reset_input_buffer()
        ser.write(reset_request)
        response = ser.read(7)

        if len(response) == 7:
            cause = (response[3] << 8) | response[4]
            name = RESET_CAUSE_NAMES.get(cause, "UNKNOWN VALUE")
            seen[cause] = seen.get(cause, 0) + 1
            print(f"  {cause} ({name})")

            if version_seen is None:
                ser.reset_input_buffer()
                ser.write(version_request)
                vr = ser.read(7)
                if len(vr) == 7:
                    version_seen = (vr[3] << 8) | vr[4]
        else:
            print(".", end="", flush=True)

        time.sleep(POLL_INTERVAL)

    ser.close()

    print()
    print()
    if version_seen is not None:
        print(f"Firmware version: v{(version_seen >> 8) & 0xFF}.{version_seen & 0xFF}")

    if not seen:
        print("No answer at all during the whole window.")
        return

    print("Reset causes observed:")
    for cause, count in sorted(seen.items()):
        print(f"  {cause} ({RESET_CAUSE_NAMES.get(cause, '?')}) - {count}x")

    if 3 in seen:
        print()
        print("OK - the watchdog fired and the cause was logged to Flash")


if __name__ == "__main__":
    main()
