# PDQManager — User Guide

> **Version**: 1.0.0
> **Platform**: Windows, macOS, Linux
> **Python**: 3.10+

PDQManager is a fleet management tool for monitoring and configuring multiple PDQminer ESP32 devices on your local network. It provides a web-based dashboard and a CLI for scanning, monitoring, and exporting device data.

---

## Table of Contents

- [Quick Start](#quick-start)
- [Installation](#installation)
- [Web Dashboard](#web-dashboard)
  - [Fleet Dashboard](#fleet-dashboard)
  - [Device Detail](#device-detail)
  - [Settings](#settings)
- [CLI Commands](#cli-commands)
  - [Default (Web Server)](#default-web-server)
  - [scan](#scan)
  - [export](#export)
- [Device Discovery](#device-discovery)
- [Authentication](#authentication)
- [REST API Reference](#rest-api-reference)
- [Troubleshooting](#troubleshooting)
- [Development](#development)

---

## Quick Start

```bash
# Install
cd tools/pdqmanager
pip install -e .

# Launch — opens the dashboard in your browser
pdqmanager
```

PDQManager starts a local web server on `http://127.0.0.1:5000` and automatically discovers PDQminer devices on your network via mDNS.

---

## Installation

### From Source (Recommended)

```bash
cd tools/pdqmanager
python3 -m venv .venv
source .venv/bin/activate   # macOS/Linux
# .venv\Scripts\activate    # Windows

pip install -e .
```

### Verify Installation

```bash
pdqmanager --version
# pdqmanager, version 1.0.0
```

### Dependencies

| Package      | Version | Purpose                          |
|-------------|---------|----------------------------------|
| flask       | ≥ 3.0   | Web server and API               |
| flask-cors  | ≥ 4.0   | Cross-origin request support     |
| zeroconf    | ≥ 0.131 | mDNS device discovery            |
| requests    | ≥ 2.31  | HTTP client for device APIs      |
| pydantic    | ≥ 2.0   | Data validation and models       |
| apscheduler | ≥ 3.10  | Background task scheduling       |
| rich        | ≥ 13.0  | Terminal output formatting       |

---

## Web Dashboard

### Fleet Dashboard

The main page shows an overview of all discovered miners.

**URL**: `http://127.0.0.1:5000/`

- **Stats Grid**: Total devices, online count, combined hashrate, total shares
- **Scan Network**: Button triggers mDNS discovery for new devices
- **Refresh**: Manually refresh device status (also auto-refreshes every 10 seconds)
- **Device Cards**: Click any device card to view its detail page

### Device Detail

**URL**: `http://127.0.0.1:5000/device/<device_id>`

Shows detailed status for a single miner:

- **Hashrate**: Current mining speed (KH/s)
- **Uptime**: Time since last boot
- **Shares**: Accepted / Rejected count
- **Pool**: Connection status

#### Authenticating

To access device configuration and management:

1. Enter the device management password (set during initial device configuration)
2. Click **Authenticate**
3. On success, the configuration form appears

#### Editing Configuration

After authentication, you can modify:

| Field        | Description                           | Example                      |
|-------------|---------------------------------------|------------------------------|
| Primary Pool | Mining pool hostname                  | `pool.nerdminers.org`        |
| Primary Port | Mining pool port                      | `3333`                       |
| Backup Pool  | Failover pool hostname (optional)     | `public-pool.io`             |
| Backup Port  | Failover pool port (optional)         | `3333`                       |
| Wallet       | Bitcoin wallet address                | `bc1q...`                    |
| Worker       | Worker name for pool identification   | `miner1`                     |

Click **Save Configuration** to push changes to the device.

#### Restarting a Device

Click **Restart Device** (requires authentication) to remotely reboot the miner. A confirmation dialog appears before the command is sent.

### Settings

**URL**: `http://127.0.0.1:5000/settings`

- **Auto-refresh interval**: Control how often the dashboard polls for updates (default 10 seconds)
- **Scan Now**: Trigger network discovery
- **About**: Version info and project link

---

## CLI Commands

### Default (Web Server)

Running `pdqmanager` without a subcommand starts the web server.

```bash
pdqmanager [OPTIONS]
```

| Option         | Type | Default | Description                         |
|---------------|------|---------|-------------------------------------|
| `--port`      | INT  | 5000    | Web server port                     |
| `--no-browser`| FLAG | off     | Don't auto-open browser on launch   |

**Examples:**

```bash
# Start on default port, opens browser
pdqmanager

# Start on custom port, no browser
pdqmanager --port 8080 --no-browser
```

Press `Ctrl+C` to stop the server. Discovery is automatically cleaned up on exit.

---

### `scan`

Quick network scan without starting the web server. Discovers devices via mDNS for 5 seconds.

```bash
pdqmanager scan
```

**Example output:**

```
Scanning for PDQminer devices (5 seconds)...

Found 3 device(s):

  PDQ_ABC123  192.168.1.100:80  v1.0.0  (ESP32)
  PDQ_DEF456  192.168.1.101:80  v1.0.0  (ESP32)
  PDQ_GHI789  192.168.1.102:80  v1.0.0  (ESP32)
```

---

### `export`

Export device data to a file after a 5-second discovery scan.

```bash
pdqmanager export [OPTIONS]
```

| Option     | Type   | Default | Description                    |
|-----------|--------|---------|--------------------------------|
| `--format`| CHOICE | json    | Output format: `json` or `csv` |
| `--output`| PATH   | stdout  | Output file path               |

**Examples:**

```bash
# Export to JSON file
pdqmanager export --format json --output fleet.json

# Export to CSV
pdqmanager export --format csv --output fleet.csv

# Print JSON to terminal
pdqmanager export
```

---

## Device Discovery

PDQManager discovers miners via **mDNS** (multicast DNS) on the local network.

### How It Works

1. PDQManager broadcasts a query for `_pdqminer._tcp.local.` services
2. Each PDQminer device responds with its IP, port, and TXT records
3. TXT records include: `id`, `version`, `hardware`
4. Discovered devices are automatically added to the dashboard
5. Removed devices are cleaned up when their mDNS advertisement expires

### Requirements

- PDQManager and miners must be on the **same local network subnet**
- mDNS traffic (UDP port 5353) must not be blocked by firewall
- Device firmware must advertise the `_pdqminer._tcp.local.` service

---

## Authentication

PDQManager communicates with each PDQminer device over HTTP on the local network.

- **Public endpoints** (no auth): `/api/status`, `/api/info`
- **Protected endpoints** (auth required): `/api/config`, `/api/restart`

### Authentication Flow

1. User enters the device password in the dashboard
2. PDQManager sends the password to the device's `/api/auth` endpoint
3. Device returns a **Bearer token** if the password is correct
4. All subsequent protected requests include the token in the `Authorization` header
5. Tokens expire after a configured period (device-side setting)

> **Note**: Authentication is per-device. Each miner has its own password set during initial configuration.

---

## REST API Reference

PDQManager exposes its own REST API for programmatic access:

### Devices

| Method | Endpoint                          | Description                    |
|--------|-----------------------------------|--------------------------------|
| GET    | `/api/devices`                    | List all devices with status   |
| GET    | `/api/devices/<id>`               | Get single device status       |
| GET    | `/api/devices/<id>/config`        | Get device config (auth req.)  |
| PUT    | `/api/devices/<id>/config`        | Update device config           |
| POST   | `/api/devices/<id>/auth`          | Authenticate with device       |
| POST   | `/api/devices/<id>/restart`       | Restart device (auth req.)     |

### Fleet

| Method | Endpoint     | Description                          |
|--------|-------------|--------------------------------------|
| GET    | `/api/stats` | Aggregate fleet statistics           |
| POST   | `/api/scan`  | Trigger network scan for new devices |

### Example: Authenticate and Get Config

```bash
# Authenticate
curl -X POST http://127.0.0.1:5000/api/devices/PDQ_ABC/auth \
  -H "Content-Type: application/json" \
  -d '{"password": "your-device-password"}'

# Response: {"status": "authenticated"}

# Get fleet stats
curl http://127.0.0.1:5000/api/stats

# Response: {"total_devices": 3, "online_devices": 3, "total_hashrate_khs": 2955.0, ...}
```

---

## Troubleshooting

### No devices found

- Ensure miners are on the **same network** as PDQManager
- Check that miners are running firmware with mDNS advertisement enabled
- Try `pdqmanager scan` to verify mDNS is working
- Check firewall settings — mDNS uses UDP port 5353
- On macOS, mDNS is natively supported. On Linux, ensure `avahi-daemon` is running

### Dashboard shows devices as offline

- The device may have lost WiFi or pool connection
- Click **Refresh** to re-poll devices
- Check the device's physical display for error messages
- Verify pool is reachable: `ping pool.nerdminers.org`

### Authentication fails

- Verify the password matches what was set during device setup
- Check the device is reachable: `curl http://<device-ip>/api/status`
- Device may have rate limiting — wait 5 minutes and try again

### Port 5000 already in use

```bash
pdqmanager --port 8080
```

---

## Development

### Run Tests

```bash
cd tools/pdqmanager
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
pytest -v
```

### Test Coverage

| Module        | Tests | Coverage |
|---------------|-------|----------|
| discovery.py  | 6     | 100%     |
| device.py     | 10    | 100%     |
| api.py        | 12    | 100%     |
| **Total**     | **28**| **100%**  |

### Code Quality

```bash
# Format
black pdqmanager/ tests/ --check

# Lint
ruff check pdqmanager/ tests/

# Type check
mypy pdqmanager/
```

### Architecture

```
pdqmanager/
├── models.py       # Pydantic data models (DeviceStatus, DeviceConfig, etc.)
├── discovery.py    # mDNS service listener (zeroconf)
├── device.py       # HTTP client for device REST APIs
├── manager.py      # Device manager (coordinates discovery + polling)
├── api.py          # Flask Blueprint — REST API endpoints
├── app.py          # Flask application factory
├── cli.py          # Click CLI (web server, scan, export)
├── templates/      # Jinja2 HTML templates
│   ├── base.html   # Navigation layout
│   ├── index.html  # Fleet dashboard
│   ├── device.html # Device detail + config
│   └── settings.html
└── static/
    ├── css/style.css  # Dark theme (Bitcoin-orange accent)
    └── js/app.js      # Client-side dashboard logic
```

---

## License

MIT — see the root [LICENSE](../../LICENSE) file.
