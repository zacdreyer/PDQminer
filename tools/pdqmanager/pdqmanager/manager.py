"""Device manager — coordinates discovery, polling, and device communication."""

from __future__ import annotations

import logging
import threading
from typing import Any

from pdqmanager.device import DeviceClient
from pdqmanager.discovery import PDQMinerDiscovery
from pdqmanager.models import DiscoveredDevice

logger = logging.getLogger(__name__)


class DeviceManager:
    """Manages all known PDQminer devices.

    Coordinates mDNS discovery, periodic status polling, and per-device
    communication clients.
    """

    def __init__(self) -> None:
        self._clients: dict[str, DeviceClient] = {}
        self._status_cache: dict[str, dict[str, Any]] = {}
        self._lock = threading.Lock()
        self._discovery: PDQMinerDiscovery | None = None

    def start_discovery(self) -> None:
        """Begin mDNS discovery for PDQminer devices."""
        self._discovery = PDQMinerDiscovery(on_device_found=self._on_device_found)
        self._discovery.start()

    def stop_discovery(self) -> None:
        """Stop mDNS discovery."""
        if self._discovery:
            self._discovery.stop()

    def _on_device_found(self, device: DiscoveredDevice) -> None:
        """Callback when a new device is discovered."""
        with self._lock:
            if device.device_id not in self._clients:
                client = DeviceClient(device.ip_address, device.port)
                self._clients[device.device_id] = client
                logger.info(
                    "Added device %s @ %s:%d", device.device_id, device.ip_address, device.port
                )
        # Fetch initial status outside the lock (does network I/O + acquires lock internally)
        if device.device_id in self._clients:
            self._poll_device(device.device_id, self._clients[device.device_id])

    def add_device_manual(self, ip_address: str, port: int = 80) -> str | None:
        """Manually add a device by IP address.

        Returns:
            Device ID if successful, None otherwise.
        """
        client = DeviceClient(ip_address, port)
        try:
            status = client.get_status()
            device_id = status.get("device_id", f"manual_{ip_address}")
            with self._lock:
                self._clients[device_id] = client
                self._status_cache[device_id] = status
            return device_id
        except Exception as e:
            logger.error("Failed to add device at %s: %s", ip_address, e)
            return None

    def _poll_device(self, device_id: str, client: DeviceClient) -> None:
        """Poll a single device for current status."""
        try:
            data = client.get_status()
            data["ip_address"] = client.ip_address
            data["authenticated"] = client.token is not None
            with self._lock:
                self._status_cache[device_id] = data
        except Exception as e:
            logger.warning("Failed to poll %s: %s", device_id, e)
            with self._lock:
                if device_id in self._status_cache:
                    self._status_cache[device_id]["pool_connected"] = False

    def refresh_all(self) -> None:
        """Poll all known devices for current status."""
        with self._lock:
            clients = dict(self._clients)
        for device_id, client in clients.items():
            self._poll_device(device_id, client)

    def get_all_status(self) -> list[dict[str, Any]]:
        """Get cached status for all devices.

        Returns:
            List of status dicts.
        """
        with self._lock:
            return list(self._status_cache.values())

    def get_device_status(self, device_id: str) -> dict[str, Any] | None:
        """Get cached status for a single device."""
        with self._lock:
            return self._status_cache.get(device_id)

    def get_device_config(self, device_id: str) -> dict[str, Any] | None:
        """Get configuration from a device (requires prior authentication)."""
        with self._lock:
            client = self._clients.get(device_id)
        if not client or not client.token:
            return None
        config = client.get_config()
        return config.model_dump() if config else None

    def update_device_config(self, device_id: str, config: dict[str, Any]) -> bool:
        """Update configuration on a device."""
        with self._lock:
            client = self._clients.get(device_id)
        if not client or not client.token:
            return False
        return client.update_config(config)

    def authenticate_device(self, device_id: str, password: str) -> bool:
        """Authenticate with a device.

        Args:
            device_id: Target device ID.
            password: Device management password.

        Returns:
            True if authentication succeeded.
        """
        with self._lock:
            client = self._clients.get(device_id)
        if not client:
            return False
        return client.authenticate(password)

    def restart_device(self, device_id: str) -> bool:
        """Send restart command to a device."""
        with self._lock:
            client = self._clients.get(device_id)
        if not client or not client.token:
            return False
        return client.restart()
