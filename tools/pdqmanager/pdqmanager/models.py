"""Pydantic data models for PDQManager."""

from __future__ import annotations

from datetime import datetime

from pydantic import BaseModel, Field, field_validator


class DeviceStatus(BaseModel):
    """Real-time status of a single PDQminer device."""

    device_id: str
    ip_address: str
    firmware_version: str = ""
    uptime_seconds: int = 0
    hashrate_khs: float = 0.0
    shares_accepted: int = 0
    shares_rejected: int = 0
    pool_connected: bool = False
    free_heap: int = 0
    last_seen: datetime = Field(default_factory=datetime.now)
    authenticated: bool = False


class DeviceConfig(BaseModel):
    """Configuration of a PDQminer device (requires authentication)."""

    wifi_ssid: str = ""
    pool1_host: str = ""
    pool1_port: int = 3333
    pool2_host: str | None = None
    pool2_port: int | None = None
    wallet: str = ""
    worker: str = ""

    @field_validator("pool1_port", "pool2_port")
    @classmethod
    def validate_port(cls, v: int | None) -> int | None:
        if v is not None and not 1 <= v <= 65535:
            raise ValueError("Port must be 1-65535")
        return v


class FleetStats(BaseModel):
    """Aggregate statistics across all managed devices."""

    total_devices: int = 0
    online_devices: int = 0
    total_hashrate_khs: float = 0.0
    total_shares_accepted: int = 0
    avg_hashrate_khs: float = 0.0


class DiscoveredDevice(BaseModel):
    """Device found via mDNS discovery."""

    device_id: str
    ip_address: str
    port: int = 80
    version: str = ""
    hardware: str = ""
