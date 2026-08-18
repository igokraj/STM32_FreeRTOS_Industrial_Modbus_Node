import serial
import time

# **** MODBUS RESPONSE LATENCY TEST ****
#
# Measures round-trip time of many consecutive read requests.
#
# Why this matters: the HTU21D read blocks for ~50 ms once per second (and up to
# ~250 ms when the sensor does not answer). In the bare-metal superloop that
# blocking stalled the whole device, so a request landing in that window waited
# for the sensor to finish. After splitting SensorTask out, ModbusTask has a
# higher priority and preempts it - the delay should disappear from the results.

PORT = "COM3"  # change to your ST-Link Virtual COM port (Device Manager -> Ports (COM & LPT))
BAUDRATE = 115200

SLAVE_ADDRESS = 1
READ_HOLDING_REGISTERS = 0x03

REQUEST_COUNT = 100
GAP_BETWEEN_REQUESTS = 0.02  # 20 ms - spreads the requests over ~2 s, so several
                             # of them are bound to hit a sensor read window
OUTLIER_THRESHOLD_MS = 20    # anything slower than this suggests the task was blocked


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


def build_read_request(slave_address: int, reg_address: int) -> bytes:
    payload = bytes([
        slave_address,
        READ_HOLDING_REGISTERS,
        (reg_address >> 8) & 0xFF,
        reg_address & 0xFF,
        0x00,
        0x01,
    ])
    crc = crc16_modbus(payload)
    return payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def main():
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    time.sleep(0.1)  # give the port a moment to open

    # Register 1 (limit switch) is deliberate: its value never depends on the
    # sensor, so any delay we measure comes from task scheduling, not from I2C.
    request = build_read_request(SLAVE_ADDRESS, reg_address=1)

    print(f"Sending {REQUEST_COUNT} requests, {int(GAP_BETWEEN_REQUESTS * 1000)} ms apart...")
    print()

    latencies = []
    no_answer = 0

    for _ in range(REQUEST_COUNT):
        ser.reset_input_buffer()
        start = time.perf_counter()
        ser.write(request)
        response = ser.read(7)
        elapsed_ms = (time.perf_counter() - start) * 1000

        if len(response) == 7:
            latencies.append(elapsed_ms)
        else:
            no_answer += 1

        time.sleep(GAP_BETWEEN_REQUESTS)

    ser.close()

    if not latencies:
        print("FAIL - no responses at all. Is the device in FAULT state?")
        return

    latencies.sort()
    outliers = [x for x in latencies if x > OUTLIER_THRESHOLD_MS]

    print(f"Responses:  {len(latencies)}/{REQUEST_COUNT}   (no answer: {no_answer})")
    print(f"Min:        {latencies[0]:.1f} ms")
    print(f"Median:     {latencies[len(latencies) // 2]:.1f} ms")
    print(f"Max:        {latencies[-1]:.1f} ms")
    print(f"Over {OUTLIER_THRESHOLD_MS} ms:  {len(outliers)}")
    print()

    if outliers:
        print(f"    Slowest responses: {', '.join(f'{x:.0f}' for x in outliers[-5:])} ms")
        print("    -> some requests waited for something else to finish")
    else:
        print("    OK - no request was blocked by the sensor read")


if __name__ == "__main__":
    main()
