<p align="center">
  <img src="docs/assets/logo-pdqminer.png" alt="PDQminer Logo" width="512"/>
</p>

<h1 align="center">PDQminer</h1>

<p align="center">
  <strong>Pretty Damn Quick Miner</strong><br/>
  High-performance open-source Bitcoin mining firmware for ESP32
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#pdqflasher">Flasher</a> •
  <a href="#pdqmanager">Manager</a> •
  <a href="#supported-hardware">Hardware</a> •
  <a href="#installation">Installation</a> •
  <a href="#project-roadmap">Roadmap</a> •
  <a href="#contributing">Contributing</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-orange" alt="Status"/>
  <img src="https://img.shields.io/badge/platform-ESP32-blue" alt="Platform"/>
  <img src="https://img.shields.io/badge/license-GPL--3.0-green" alt="License"/>
  <img src="https://img.shields.io/badge/hashrate-≥1000%20KH%2Fs-brightgreen" alt="Hashrate"/>
</p>

---

## What is PDQminer?

PDQminer is a **fully open-source** Bitcoin mining ecosystem for ESP32 microcontrollers, consisting of:

1. **PDQminer Firmware** - Maximum hashrate mining firmware (≥1000 KH/s)
2. **PDQFlasher** - Cross-platform firmware flashing tool with auto-detection
3. **PDQManager** - Fleet management application for monitoring multiple miners

> **Goal**: Exceed 1000 KH/s on ESP32-D0 hardware while remaining 100% transparent, secure, and community-driven.

### Why PDQminer?

| Aspect | NerdMiner | NMMiner | PDQminer |
|--------|-----------|---------|----------|
| **Open Source** | ✅ Yes | ❌ Closed | ✅ 100% Open |
| **Hashrate (ESP32-D0)** | ~55 KH/s | ~1000 KH/s | **≥1000 KH/s** |
| **Configuration** | Captive Portal | Captive Portal | Captive Portal |
| **Fleet Management** | ❌ No | ❌ No | ✅ **PDQManager** |
| **Auto-Flash Tool** | ❌ No | ✅ Yes | ✅ **PDQFlasher** |
| **Security Focused** | Basic | Unknown | ✅ **PBKDF2 + Rate Limiting** |

---

## Features

### PDQminer Firmware

- **Maximum Hashrate SHA256d Engine**
  - Midstate precomputation (first 64 bytes cached)
  - Nonce-only block updates (16 bytes per hash)
  - Fully unrolled SHA256 rounds (no loop overhead)
  - IRAM-optimized hot paths
  - Hardware SHA256 acceleration (ESP32-S3)
  - Early hash rejection (check MSB first)

- **Dual-Core Architecture**
  - Core 1: 100% dedicated to mining
  - Core 0: WiFi, display, stratum communication
  - Zero contention design - no mutex in hash loop

- **Stratum V1 Protocol**
  - Pool mining with automatic reconnection
  - Share submission and difficulty tracking
  - Pool failover with backup pool support

- **Minimalistic Display Mode**
  - 5-second update interval (configurable)
  - Text-only stats (hashrate, shares, uptime)
  - Zero GPU acceleration needed
  - Maximum CPU cycles for mining

- **Captive Portal Configuration**
  - WiFi SSID/Password
  - Primary and backup pool settings
  - BTC wallet address and worker name
  - Timezone and display preferences
  - Settings stored encrypted in NVS

### PDQFlasher

Cross-platform firmware flashing tool with GUI and CLI:

- **Auto Board Detection**
  - ESP32-D0, ESP32-S3, ESP32-C3 identification
  - Display controller detection (ILI9341/ST7789)
  - Automatic firmware selection

- **One-Click Flashing**
  - Built-in esptool integration
  - Progress bar and status updates
  - Verify after flash option

- **Cross-Platform**
  - Windows, macOS, Linux support
  - GUI (DearPyGUI) and CLI modes
  - No additional drivers required

### PDQManager

Fleet management application for monitoring multiple miners:

- **mDNS Discovery**
  - Automatic miner discovery on local network
  - Real-time device status updates
  - No manual IP configuration

- **Fleet Monitoring**
  - Combined hashrate dashboard
  - Per-device statistics
  - Historical performance graphs

- **Remote Configuration**
  - Pool settings management
  - Firmware update coordination
  - Device restart/reset controls

- **Secure Communication**
  - Per-device password authentication
  - Token-based API sessions
  - Rate limiting on auth endpoints

---

## Supported Hardware

### Currently Supported

| Board | Display | Driver | Build Target | Status |
|-------|---------|--------|--------------|--------|
| ESP32-2432S028R | 2.8" TFT | ILI9341 | `cyd_ili9341` | In Development |
| ESP32-2432S028R | 2.8" TFT | ST7789 | `cyd_st7789` | In Development |

### Planned Support

| Board | Display | Status |
|-------|---------|--------|
| ESP32-2432S028C | 2.8" TFT (Capacitive) | Planned |
| ESP32-2432S024 | 2.4" TFT | Planned |
| ESP32-3248S035 | 3.5" TFT | Planned |
| ESP32-WROOM-32 | Various | Planned |
| LILYGO T-Display S3 | 1.9" AMOLED | Planned |

> **Note**: The ESP32-2432S028R (CYD - Cheap Yellow Display) exists in two variants with different display drivers. PDQFlasher auto-detects your variant.

---

## Installation

### Pre-built Firmware (Recommended)

1. Download PDQFlasher from [Releases](../../releases)
2. Connect your ESP32 board via USB
3. Run PDQFlasher and click "Flash"

### Manual Flashing

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 \
  --baud 921600 write_flash \
  0x1000 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 pdqminer.bin
```

### First Boot Configuration

1. Power on the device - display shows "PDQminer Setup"
2. Connect to WiFi network: `PDQminer-XXXX` (password: `pdqminer123`)
3. Browser opens automatically (or navigate to `192.168.4.1`)
4. Configure:
   - WiFi network credentials
   - Primary pool: `public-pool.io:21496` (recommended)
   - Backup pool: `solo.ckpool.org:3333` (optional)
   - BTC wallet address
   - Worker name (optional)
5. Click "Save & Reboot"
6. Device connects to WiFi and starts mining

---

## Building from Source

### Prerequisites

- [PlatformIO](https://platformio.org/) (recommended)
- or ESP-IDF v5.x
- Python 3.10+ (for tools)

### Build Firmware

```bash
git clone https://github.com/zacdreyer/PDQminer.git
cd PDQminer

# Build for CYD with ILI9341 display
pio run -e cyd_ili9341

# Build for CYD with ST7789 display
pio run -e cyd_st7789

# Upload to connected board
pio run -e cyd_ili9341 -t upload

# Run tests
pio test -e native
```

### Build PDQFlasher

```bash
cd tools/pdqflasher
pip install -e ".[gui]"
pdqflasher --gui  # Launch GUI
pdqflasher --port /dev/ttyUSB0  # CLI mode
```

### Build PDQManager

```bash
cd tools/pdqmanager
pip install -e .
pdqmanager  # Starts web server at http://localhost:5000
```

---

## Performance Targets

| Board | Clock | Expected Hashrate | Status |
|-------|-------|-------------------|--------|
| ESP32-D0WD-V3 | 240 MHz | **≥1000 KH/s** | In Development |
| ESP32-S3 | 240 MHz | ~400 KH/s | Planned |
| ESP32-C3 | 160 MHz | ~200 KH/s | Planned |

### Optimization Techniques

1. **Midstate Caching**: First SHA256 block computed once per job
2. **Nonce-Only Updates**: Only 4 bytes change per hash attempt
3. **Loop Unrolling**: 64 SHA256 rounds fully unrolled
4. **Early Rejection**: Check hash MSB before full comparison
5. **IRAM Placement**: Critical code in fast instruction RAM
6. **Zero Allocation**: No malloc/free in mining loop
7. **Minimal Display**: 5-second refresh, text-only

---

## Project Roadmap

### Phase 1: Foundation (Complete)

- [x] Project architecture and documentation
- [x] Software Design Document (SDD)
- [x] Test-Driven Development Guide (TDD)
- [x] Coding standards with security/memory best practices
- [x] PDQManager specification
- [x] PDQFlasher specification
- [x] Project scaffolding and build system

### Phase 2: Core Development (In Progress)

- [x] SHA256 engine with midstate optimization
- [x] Dual-core mining task implementation
- [x] Hardware abstraction layer (HAL)
- [x] Benchmark firmware
- [x] Stratum V1 client implementation
- [x] WiFi manager and captive portal
- [x] NVS configuration storage
- [x] Phase B code review - Round 1 (security, accuracy, optimization)
- [x] Phase B code review - Round 2 (comprehensive review, 9 issues fixed)
- [x] Phase B code review - Round 3 (critical share submission fix, 4 issues fixed)
- [x] Phase B code review - Round 4 (100% confidence review, 5 critical bugs fixed)
- [ ] Basic display driver (ILI9341/ST7789)

### Phase 3: Optimization

- [x] Performance benchmarking framework
- [ ] SHA256 hot path profiling
- [ ] Hardware SHA256 integration (ESP32-S3)
- [ ] Memory usage optimization
- [ ] Achieve ≥1000 KH/s target

### Phase 4: Tools

- [ ] PDQFlasher CLI implementation
- [ ] PDQFlasher GUI implementation
- [ ] PDQManager backend (Flask)
- [ ] PDQManager frontend (web UI)
- [ ] mDNS discovery implementation

### Phase 5: Release

- [ ] Beta testing program
- [ ] Documentation finalization
- [ ] v1.0.0 release
- [ ] Additional board support

---

## Project Structure

```
PDQminer/
├── src/                        # Firmware source code
│   ├── core/                   # SHA256 engine, mining tasks
│   ├── stratum/                # Pool communication
│   ├── network/                # WiFi management
│   ├── display/                # Display drivers
│   └── hal/                    # Hardware abstraction
├── test/                       # Unit & integration tests
│   ├── unit/                   # Isolated unit tests
│   ├── integration/            # Component integration tests
│   ├── benchmark/              # Performance benchmarks
│   └── mocks/                  # Mock implementations
├── tools/                      # PC tools
│   ├── pdqflasher/             # Firmware flashing tool
│   └── pdqmanager/             # Fleet management tool
├── boards/                     # Board configurations
├── docs/                       # Documentation
│   ├── sdd.md                  # Software Design Document
│   ├── tdd.md                  # Test-Driven Development Guide
│   ├── coding-standards.md     # Coding standards
│   └── agents/                 # AI agent documentation
├── platformio.ini              # Build configuration
└── README.md                   # This file
```

---

## Security

PDQminer is designed with security as a priority:

- **Password Storage**: PBKDF2-SHA256 with 100,000 iterations and random salt
- **API Authentication**: Token-based with 5-minute expiry
- **Rate Limiting**: 5 attempts per 5 minutes on auth endpoints
- **Input Validation**: All external input validated and sanitized
- **Buffer Safety**: No strcpy/sprintf, all buffers bounds-checked
- **Memory Safety**: Sensitive data zeroed after use

See [Security Considerations](docs/sdd.md#security-considerations) for details.

---

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting PRs.

### Development Setup

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-optimization`
3. Follow the [coding standards](docs/coding-standards.md)
4. Write tests for new functionality
5. Ensure all tests pass: `pio test -e native`
6. Submit a pull request

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `chore`

Example:
```
perf(sha256): optimize midstate computation

- Precompute first 64 bytes of message schedule
- Reduce per-nonce computation by 50%
- Measured improvement: 340 KH/s → 680 KH/s

Closes #42
```

---

## Acknowledgments

- [NerdMiner](https://github.com/BitMaker-hub/NerdMiner_v2) - Inspiration and reference implementation
- [public-pool.io](https://web.public-pool.io) - Low difficulty pool for small miners
- ESP-IDF and PlatformIO communities

---

## License

GNU General Public License v3.0 - see [LICENSE](LICENSE) for details.

---

## Disclaimer

Solo mining with an ESP32 is extremely unlikely to find a Bitcoin block. This project is for educational purposes and entertainment. The odds of finding a block with 1000 KH/s are approximately 1 in 10^18 per year.

---

<p align="center">
  <strong>⚡ Mine Bitcoin. Open Source. Pretty Damn Quick. ⚡</strong>
</p>
