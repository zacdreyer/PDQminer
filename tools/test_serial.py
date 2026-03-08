#!/usr/bin/env python3
"""Quick test: flash, manual-reset, capture boot output + mining results."""
import subprocess, serial, time, os, sys

ESPTOOL = os.path.expanduser('~/.platformio/packages/tool-esptoolpy/esptool.py')
WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE = os.path.join(WORKSPACE, '.pio', 'build', 'cyd_ili9341', 'firmware.bin')
port = '/dev/cu.usbserial-130'

# Duration from command line, default 20 seconds
duration = int(sys.argv[1]) if len(sys.argv) > 1 else 20

# Flash with no_reset
print("Flashing...")
proc = subprocess.run(
    ['python3', ESPTOOL, '--port', port, '--baud', '460800',
     '--chip', 'esp32', '--after', 'no_reset',
     'write_flash', '0x10000', FIRMWARE],
    capture_output=True, text=True, timeout=30
)
print(f'Flash: {"OK" if proc.returncode == 0 else "FAIL"}')
if proc.returncode != 0:
    print(proc.stderr[-500:])
    exit(1)

# Open serial FIRST, then reset
time.sleep(0.2)
ser = serial.Serial(port, 115200, timeout=1)

# Hardware reset via RTS
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.1)
ser.setRTS(False)
ser.setDTR(False)
time.sleep(0.1)
ser.reset_input_buffer()

print(f"Reading serial for {duration}s...")
deadline = time.time() + duration
sw_pass = 0
sw_fail = 0
hashrate_line = None
while time.time() < deadline:
    raw = ser.readline()
    if raw:
        line = raw.decode('utf-8', errors='ignore').strip()
        if line:
            print(f"  {line}")
        if 'SW verify: PASS' in line:
            sw_pass += 1
        if 'SW verify: FAIL' in line:
            sw_fail += 1
        if 'Hashrate' in line and 'KH/s' in line:
            hashrate_line = line.strip()

ser.close()
print(f"\n--- Summary ---")
print(f"SW verify: {sw_pass} PASS, {sw_fail} FAIL")
if hashrate_line:
    print(f"Last hashrate: {hashrate_line}")
print("Done")
