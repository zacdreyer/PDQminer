# PDQFlasher — User Guide

> **Version**: 1.0.0
> **Platform**: Windows, macOS, Linux
> **Python**: 3.10+

PDQFlasher is a cross-platform CLI tool for flashing firmware to PDQminer ESP32 devices. It auto-detects connected boards, selects the correct firmware, and handles all flashing operations via [esptool](https://github.com/espressif/esptool).

---

## Table of Contents

- [Quick Start](#quick-start)
- [Installation](#installation)
- [Commands](#commands)
  - [flash](#flash)
  - [detect](#detect)
  - [erase](#erase)
- [Supported Hardware](#supported-hardware)
- [Troubleshooting](#troubleshooting)
- [Development](#development)

---

## Quick Start

```bash
# Install
cd tools/pdqflasher
pip install -e .

# Plug in your ESP32, then:
pdqflash flash --binary firmware.bin
```

PDQFlasher auto-detects the serial port and board type. One command is all you need.

---

## Installation

### From Source (Recommended)

```bash
cd tools/pdqflasher
python3 -m venv .venv
source .venv/bin/activate   # macOS/Linux
# .venv\Scripts\activate    # Windows

pip install -e .
```

### Verify Installation

```bash
pdqflash --version
# pdqflasher, version 1.0.0
```

### Dependencies

| Package  | Version | Purpose                |
|----------|---------|------------------------|
| esptool  | ≥ 4.0   | ESP32 flash operations |
| pyserial | ≥ 3.5   | Serial port access     |
| click    | ≥ 8.0   | CLI framework          |
| rich     | ≥ 13.0  | Terminal output        |

---

## Commands

### `flash`

Flash firmware to a connected PDQminer device.

```bash
pdqflash flash [OPTIONS]
```

| Option     | Type   | Default     | Description                         |
|------------|--------|-------------|-------------------------------------|
| `--binary` | PATH   | (per board) | Path to firmware `.bin` file        |
| `--board`  | CHOICE | auto-detect | Board type: `cyd_ili9341`, `cyd_st7789` |
| `--port`   | TEXT   | auto-detect | Serial port (e.g. `/dev/ttyUSB0`)   |
| `--baud`   | INT    | 460800      | Serial baud rate                    |
| `--verify` | FLAG   | off         | Verify flash contents after writing |
| `--erase`  | FLAG   | off         | Erase flash before writing          |

**Examples:**

```bash
# Auto-detect everything, flash specific binary
pdqflash flash --binary build/pdqminer.bin

# Specify port and board explicitly
pdqflash flash --binary firmware.bin --port /dev/cu.usbserial-130 --board cyd_ili9341

# Flash with verification and erase
pdqflash flash --binary firmware.bin --verify --erase

# Lower baud rate for unreliable connections
pdqflash flash --binary firmware.bin --baud 115200
```

**Workflow:**
1. Auto-detects serial port (scans for known ESP32 USB VID/PID pairs)
2. Auto-detects board type (reads chip ID via esptool)
3. Optionally erases flash (`--erase`)
4. Flashes firmware at the specified baud rate
5. Optionally verifies flash contents (`--verify`)

---

### `detect`

Detect connected ESP32 devices and display information.

```bash
pdqflash detect [OPTIONS]
```

| Option   | Type | Default     | Description                       |
|----------|------|-------------|-----------------------------------|
| `--port` | TEXT | auto-detect | Serial port to probe              |

**Example:**

```bash
pdqflash detect
```

Output:
```
Found device on: /dev/cu.usbserial-130
        Device Information
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ Property    ┃ Value                       ┃
┡━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┩
│ Port        │ /dev/cu.usbserial-130       │
│ Chip        │ ESP32                       │
│ MAC Address │ 08:B6:1F:3B:92:D8          │
└─────────────┴─────────────────────────────┘
```

---

### `erase`

Erase the entire flash memory on an ESP32 device. This removes all firmware and configuration data (NVS).

```bash
pdqflash erase --port PORT [OPTIONS]
```

| Option   | Type | Default | Description       |
|----------|------|---------|-------------------|
| `--port` | TEXT | *required* | Serial port    |
| `--baud` | INT  | 460800  | Serial baud rate  |

**Example:**

```bash
pdqflash erase --port /dev/cu.usbserial-130
```

> **Warning**: This erases all data including WiFi credentials and pool configuration. The device will need to be reconfigured after re-flashing.

---

## Supported Hardware

### Auto-Detected USB Chips

PDQFlasher recognises these USB-to-serial chips commonly found on ESP32 boards:

| Chip         | VID    | PID    | Common Boards        |
|-------------|--------|--------|----------------------|
| CP210x      | 0x10C4 | 0xEA60 | CYD, NodeMCU         |
| CH340/CH341 | 0x1A86 | 0x7523 | CYD variants          |
| FTDI FT232R | 0x0403 | 0x6001 | Various dev boards    |
| ESP32 USB   | 0x303A | 0x1001 | ESP32-S3 native USB   |

### Supported Board Configurations

| Board ID      | Chip  | Flash | Display  | Firmware File            |
|---------------|-------|-------|----------|--------------------------|
| `cyd_ili9341` | ESP32 | 4 MB  | ILI9341  | `firmware_cyd_ili9341.bin` |
| `cyd_st7789`  | ESP32 | 4 MB  | ST7789   | `firmware_cyd_st7789.bin`  |

---

## Troubleshooting

### No ESP32 device detected

- Ensure the USB cable is a **data cable** (not charge-only)
- Check the device appears in your OS (`ls /dev/tty*` on macOS/Linux, Device Manager on Windows)
- Try specifying the port manually: `pdqflash flash --port /dev/ttyUSB0 --binary firmware.bin`
- On macOS, the port is typically `/dev/cu.usbserial-XXXX`
- Install USB drivers if needed:
  - CP210x: [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
  - CH340: [WCH](http://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html)

### Flash failed with baud rate error

Lower the baud rate:
```bash
pdqflash flash --binary firmware.bin --baud 115200
```

### Permission denied on serial port (Linux)

Add your user to the `dialout` group:
```bash
sudo usermod -a -G dialout $USER
# Log out and back in for the change to take effect
```

### Board detection works but flash fails

Try erasing first:
```bash
pdqflash flash --binary firmware.bin --erase
```

---

## Development

### Run Tests

```bash
cd tools/pdqflasher
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
pytest -v
```

### Test Coverage

| Module      | Tests | Coverage |
|-------------|-------|----------|
| config.py   | 10    | 100%     |
| detector.py | 9     | 100%     |
| flasher.py  | 12    | 100%     |
| cli.py      | 6     | 100%     |
| **Total**   | **37**| **100%**  |

### Code Quality

```bash
# Format
black pdqflasher/ tests/ --check

# Lint
ruff check pdqflasher/ tests/

# Type check
mypy pdqflasher/
```

---

## License

MIT — see the root [LICENSE](../../LICENSE) file.
