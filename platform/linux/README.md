# PDQminer Linux / Docker Build

Run the PDQminer solo Bitcoin miner as a native Linux process or Docker container.
Uses the software SHA256 path (~46 KH/s per thread) instead of the ESP32 hardware
SHA accelerator (~945 KH/s).

## Quick Start (Docker)

```bash
docker compose up --build
```

Override defaults with environment variables or a `.env` file:

```bash
# .env (place in project root or platform/linux/)
PDQ_POOL_HOST=public-pool.io
PDQ_POOL_PORT=3333
PDQ_WALLET=your_btc_address
PDQ_WORKER=docker01
PDQ_THREADS=2
```

## Quick Start (Native)

```bash
cd platform/linux
mkdir build && cd build
cmake ..
make -j$(nproc)
./pdqminer \
  --pool-host pool.nerdminers.org \
  --pool-port 3333 \
  --wallet YOUR_BTC_ADDRESS \
  --worker linux01 \
  --threads 2
```

## Command-Line Options

| Option | Default | Description |
|---|---|---|
| `--pool-host` | `pool.nerdminers.org` | Stratum pool hostname |
| `--pool-port` | `3333` | Stratum pool port |
| `--wallet` | (required) | Bitcoin wallet address |
| `--worker` | `pdqlinux` | Worker name |
| `--threads` | `2` | Number of mining threads |
| `--difficulty` | `1.0` | Suggested share difficulty |
| `--config` | (none) | JSON config file path |

## Environment Variables (Docker)

| Variable | Default | Description |
|---|---|---|
| `PDQ_POOL_HOST` | `pool.nerdminers.org` | Stratum pool hostname |
| `PDQ_POOL_PORT` | `3333` | Stratum pool port |
| `PDQ_WALLET` | (none) | Bitcoin wallet address |
| `PDQ_WORKER` | `pdqlinux` | Worker name |
| `PDQ_THREADS` | `2` | Mining thread count |
| `PDQ_DIFFICULTY` | `1.0` | Suggested share difficulty |

## Architecture

The Linux build reuses the core PDQminer source files (SHA256 engine, stratum
client, mining types) and replaces ESP32-specific components:

| ESP32 Component | Linux Replacement |
|---|---|
| FreeRTOS tasks | POSIX threads (pthread) |
| NVS flash storage | JSON config file / CLI args |
| WiFi manager | Not needed (host networking) |
| TFT display | Terminal output (headless) |
| Hardware SHA256 | Software SHA256 fallback |
| Arduino `setup()`/`loop()` | Standard `main()` |
| Watchdog timer | No-op |

## Performance

| Platform | Hashrate | Notes |
|---|---|---|
| ESP32-D0WD-V3 | ~985 KH/s | HW SHA + dual-core |
| Linux (1 thread) | ~46 KH/s | Software SHA256 |
| Linux (2 threads) | ~92 KH/s | Split nonce space |
| Docker (2 cores) | ~92 KH/s | Same as native |

The Linux build is intended for development, testing, and protocol validation
rather than competitive mining.
