# PDQminer — Docker & Native Build Guide

Run the PDQminer solo Bitcoin miner as a **Docker container**, a **native Linux
binary**, or a **native macOS binary**. This build uses the software SHA256 path
(~46 KH/s per thread) instead of the ESP32 hardware SHA accelerator (~945 KH/s).
It is intended for development, testing, and protocol validation.

---

## Table of Contents

- [Quick Start — Docker](#quick-start--docker)
- [Quick Start — macOS (Native)](#quick-start--macos-native)
- [Quick Start — Linux (Native)](#quick-start--linux-native)
- [Command-Line Reference](#command-line-reference)
- [Environment Variables](#environment-variables)
- [Configuration File](#configuration-file)
- [Running Multiple Instances](#running-multiple-instances)
- [Architecture](#architecture)
- [Troubleshooting](#troubleshooting)
- [Performance](#performance)

---

## Quick Start — Docker

> **Prerequisites**: [Docker Desktop](https://www.docker.com/products/docker-desktop/)
> (macOS, Windows, or Linux) with Docker Compose v2.

### 1. Create a `.env` file

In the project root (`PDQminer/`), create a `.env` file with your wallet:

```bash
# .env
PDQ_WALLET=bc1q_YOUR_BITCOIN_ADDRESS_HERE
PDQ_WORKER=docker01
PDQ_POOL_HOST=pool.nerdminers.org
PDQ_POOL_PORT=3333
PDQ_THREADS=2
PDQ_DIFFICULTY=1.0
```

### 2. Build and run

```bash
docker compose up --build
```

You should see output like:

```
===========================================
  PDQminer v0.1.0 (Linux)
===========================================
  Pool:       pool.nerdminers.org:3333
  Wallet:     bc1q_YOUR_BITCOIN_ADDRESS_HERE
  Worker:     docker01
  Threads:    2
  Difficulty: 1.0
===========================================

[PDQminer] Connected to pool
[PDQminer] Authorized
[PDQminer] Mining started with 2 thread(s)
[Mining] Started 2 thread(s)
[Mine-0] Thread started, nonce range 00000000-7FFFFFFF
[Mine-1] Thread started, nonce range 80000000-FFFFFFFF
[PDQminer] New job: 1a2b3c (diff=1.0)
[PDQminer] Hashrate: 92 KH/s | Shares: 3 | Blocks: 0 | Uptime: 30s
```

### 3. Stop

Press `Ctrl+C` or run:

```bash
docker compose down
```

### Run in background (detached)

```bash
docker compose up --build -d

# View logs
docker compose logs -f

# Stop
docker compose down
```

### Override settings without editing .env

```bash
PDQ_WALLET=bc1qxyz PDQ_THREADS=4 docker compose up --build
```

---

## Quick Start — macOS (Native)

### Prerequisites

Install the Xcode command-line tools and CMake:

```bash
# Xcode command-line tools (provides clang, make)
xcode-select --install

# CMake (via Homebrew)
brew install cmake
```

If you don't have Homebrew, you can compile directly without CMake — see
[Building without CMake](#building-without-cmake) below.

### Build with CMake

```bash
cd platform/linux
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

### Build without CMake

If you don't want to install CMake, compile directly with `clang`:

```bash
cd platform/linux
mkdir -p build

cc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
  -DPDQ_HEADLESS=1 -DPDQ_LINUX=1 -D_GNU_SOURCE \
  -I../../src \
  main.c linux_hal.c linux_config.c linux_wifi.c linux_display.c linux_mining.c \
  ../../src/core/sha256_engine.c \
  ../../src/stratum/stratum_client.c \
  ../../src/api/device_api.c \
  -lpthread \
  -o build/pdqminer
```

### Run

```bash
./build/pdqminer \
  --wallet bc1q_YOUR_BITCOIN_ADDRESS \
  --worker mac01 \
  --pool-host pool.nerdminers.org \
  --pool-port 3333 \
  --threads 2
```

### Stop

Press `Ctrl+C`. The miner handles SIGINT gracefully and shuts down all threads.

---

## Quick Start — Linux (Native)

### Prerequisites

```bash
# Debian / Ubuntu
sudo apt-get install build-essential cmake

# Fedora / RHEL
sudo dnf install gcc cmake make

# Arch
sudo pacman -S base-devel cmake
```

### Build

```bash
cd platform/linux
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Run

```bash
./pdqminer \
  --wallet bc1q_YOUR_BITCOIN_ADDRESS \
  --worker linux01 \
  --threads 2
```

---

## Command-Line Reference

```
Usage: pdqminer [options]
```

| Option | Short | Default | Description |
|---|---|---|---|
| `--pool-host HOST` | `-H` | `pool.nerdminers.org` | Stratum pool hostname |
| `--pool-port PORT` | `-P` | `3333` | Stratum pool port |
| `--wallet ADDR` | `-w` | *(required)* | Bitcoin wallet address |
| `--worker NAME` | `-W` | `pdqlinux` | Worker name sent to pool |
| `--threads N` | `-t` | `2` | Number of mining threads (1–32) |
| `--difficulty D` | `-d` | `1.0` | Suggested share difficulty |
| `--config FILE` | `-c` | *(none)* | Path to JSON config file |
| `--help` | `-h` | | Show help and exit |

**Examples:**

```bash
# Minimal — uses all defaults except wallet
./pdqminer --wallet bc1qxyz123

# Custom pool with 4 threads
./pdqminer -w bc1qxyz123 -W myrig -H public-pool.io -P 3333 -t 4

# Load settings from a config file
./pdqminer --config /path/to/config.json
```

---

## Environment Variables

Environment variables set defaults. CLI arguments override them.

| Variable | Default | Maps to CLI |
|---|---|---|
| `PDQ_POOL_HOST` | `pool.nerdminers.org` | `--pool-host` |
| `PDQ_POOL_PORT` | `3333` | `--pool-port` |
| `PDQ_WALLET` | *(none)* | `--wallet` |
| `PDQ_WORKER` | `pdqlinux` | `--worker` |
| `PDQ_THREADS` | `2` | `--threads` |
| `PDQ_DIFFICULTY` | `1.0` | `--difficulty` |

**Priority order** (highest wins): CLI args → Environment variables → Hardcoded defaults

**Docker-only variables** (in `docker-compose.yml`):

| Variable | Default | Description |
|---|---|---|
| `PDQ_CPU_LIMIT` | `2.0` | Max CPU cores for the container |

---

## Configuration File

The miner can persist settings in a JSON file at `~/.pdqminer/config.json`
(or a custom path via `--config` or `PDQ_CONFIG_PATH` env var).

```json
{
  "pool1_host": "pool.nerdminers.org",
  "pool1_port": "3333",
  "pool2_host": "public-pool.io",
  "pool2_port": "3333",
  "wallet": "bc1q_YOUR_ADDRESS",
  "worker": "myrig",
  "__pdq_valid__": "1346371907"
}
```

> **Note**: The config file is auto-created when you call `PdqConfigSave()` via
> the API. For normal usage, CLI args and env vars are sufficient — no config
> file is needed.

---

## Running Multiple Instances

### Docker — multiple miners

```yaml
# docker-compose.yml (custom)
services:
  miner1:
    build: .
    environment:
      - PDQ_WALLET=bc1qxyz
      - PDQ_WORKER=miner1
      - PDQ_THREADS=2
    deploy:
      resources:
        limits:
          cpus: "2.0"

  miner2:
    build: .
    environment:
      - PDQ_WALLET=bc1qxyz
      - PDQ_WORKER=miner2
      - PDQ_POOL_HOST=public-pool.io
      - PDQ_THREADS=2
    deploy:
      resources:
        limits:
          cpus: "2.0"
```

```bash
docker compose up --build -d
docker compose logs -f
```

### Native — multiple processes

```bash
./pdqminer --wallet bc1qxyz --worker rig1 --threads 2 &
./pdqminer --wallet bc1qxyz --worker rig2 --threads 2 --pool-host public-pool.io &
```

Each worker name should be unique so the pool tracks them separately.

---

## Architecture

The native build reuses the core PDQminer C source files and replaces
ESP32-specific components with platform-native equivalents:

```
┌─────────────────────────────────────────────────┐
│                   main.c                        │
│         CLI args / env vars / signals           │
├─────────────┬──────────────┬────────────────────┤
│ Stratum     │ Mining       │ Stats / API        │
│ Client      │ Threads      │                    │
│ (shared)    │ (pthread)    │ (shared stub)      │
├─────────────┼──────────────┼────────────────────┤
│ SHA256      │ Config       │ HAL                │
│ Engine      │ (JSON file)  │ (Linux/macOS)      │
│ (shared)    │              │                    │
└─────────────┴──────────────┴────────────────────┘
        │              │              │
   stratum_client.c  linux_config.c  linux_hal.c
   sha256_engine.c   linux_mining.c  linux_wifi.c
   (from src/)       linux_display.c
                     (platform/linux/)
```

| ESP32 Component | Native Replacement | File |
|---|---|---|
| FreeRTOS tasks (`xTaskCreatePinnedToCore`) | POSIX threads (`pthread_create`) | `linux_mining.c` |
| NVS flash storage (`nvs_get/set`) | JSON file (`~/.pdqminer/config.json`) | `linux_config.c` |
| WiFi manager + captive portal | Host networking (always "connected") | `linux_wifi.c` |
| TFT display (TFT_eSPI) | Headless (no-op stubs) | `linux_display.c` |
| Hardware SHA256 peripheral | Software SHA256 fallback | `sha256_engine.c` (shared) |
| Arduino `setup()`/`loop()` | Standard `main()` with `getopt_long` | `main.c` |
| Watchdog timer (`esp_task_wdt`) | No-op | `linux_hal.c` |
| Temperature sensor (`temperatureRead`) | `/sys/class/thermal` (Linux) or 0 (macOS) | `linux_hal.c` |
| Free heap (`esp_get_free_heap_size`) | `sysinfo()` (Linux) or 0 (macOS) | `linux_hal.c` |
| Chip ID (`esp_read_mac`) | Hostname-derived hash | `linux_hal.c` |

---

## Troubleshooting

### Docker: "Set PDQ_WALLET to your Bitcoin address"

The `docker-compose.yml` requires `PDQ_WALLET`. Either:
- Create a `.env` file with `PDQ_WALLET=bc1q...`
- Or pass it inline: `PDQ_WALLET=bc1q... docker compose up`

### Docker: build fails on Apple Silicon

Ensure Docker Desktop has Rosetta emulation enabled, or the build runs natively
on ARM — the C code is architecture-independent.

### macOS: `cmake: command not found`

Install via Homebrew (`brew install cmake`) or use the [direct compile
command](#building-without-cmake) with `cc` which requires no extra tools.

### macOS: `linux_hal.c` warnings about `sysinfo`

Expected. The `sysinfo()` call is Linux-only and guarded by `#if
defined(__linux__)`. On macOS, temperature and heap return 0 — mining is
unaffected.

### Connection timeout

Check that the pool host resolves and port 3333 is reachable:

```bash
nc -zv pool.nerdminers.org 3333
```

If it times out, try the backup pool:

```bash
./pdqminer --wallet bc1qxyz --pool-host public-pool.io --pool-port 3333
```

### Authorization error

The pool rejected the wallet address. Verify your Bitcoin address is valid.
Test wallets (e.g., `bc1qtest`) will be rejected by production pools.

### Low hashrate

Software SHA256 is expected to be slower than the ESP32 hardware accelerator.
Typical rates:

| Threads | Expected |
|---|---|
| 1 | ~46 KH/s |
| 2 | ~92 KH/s |
| 4 | ~184 KH/s |
| 8 | ~368 KH/s |

Adding more threads beyond your CPU core count provides no benefit.

### "Shutting down..." doesn't exit

If `Ctrl+C` shows the message but the process hangs, send SIGKILL:

```bash
kill -9 $(pgrep pdqminer)
```

---

## Performance

| Platform | Hashrate | Notes |
|---|---|---|
| ESP32-D0WD-V3 | ~1081 KH/s | HW SHA + dual-core (production) |
| Linux / macOS (1 thread) | ~46 KH/s | Software SHA256 |
| Linux / macOS (2 threads) | ~92 KH/s | Split nonce space |
| Linux / macOS (4 threads) | ~184 KH/s | 4-way nonce split |
| Docker (2 CPU) | ~92 KH/s | Same as native |

> The native build is best used for **development**, **testing**, and **protocol
> validation**. For actual mining, use the ESP32 firmware with hardware SHA256
> acceleration.
