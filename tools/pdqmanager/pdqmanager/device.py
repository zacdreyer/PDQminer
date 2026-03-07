"""HTTP client for communicating with PDQminer device REST APIs."""

from __future__ import annotations

import logging
from typing import Any

import requests

from pdqmanager.models import DeviceConfig, DeviceStatus

logger = logging.getLogger(__name__)

# Timeout for HTTP requests to devices (seconds)
_REQUEST_TIMEOUT = 5


class DeviceClient:
    """HTTP client for a single PDQminer device.

    Args:
        ip_address: Device IP address.
        port: Device HTTP port (default 80).
    """

    def __init__(self, ip_address: str, port: int = 80) -> None:
        self.ip_address = ip_address
        self.port = port
        self.base_url = f"http://{ip_address}:{port}" if port != 80 else f"http://{ip_address}"
        self.token: str | None = None

    def _headers(self) -> dict[str, str]:
        """Build request headers, including auth token if available."""
        headers: dict[str, str] = {"Content-Type": "application/json"}
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        return headers

    def get_status(self) -> dict[str, Any]:
        """Get device status (no authentication required).

        Returns:
            Dict with device status fields.

        Raises:
            requests.RequestException: On connection failure.
        """
        resp = requests.get(
            f"{self.base_url}/api/status",
            timeout=_REQUEST_TIMEOUT,
        )
        resp.raise_for_status()
        return resp.json()

    def get_info(self) -> dict[str, Any]:
        """Get device info (no authentication required).

        Returns:
            Dict with device info (version, MAC, etc.).
        """
        resp = requests.get(
            f"{self.base_url}/api/info",
            timeout=_REQUEST_TIMEOUT,
        )
        resp.raise_for_status()
        return resp.json()

    def authenticate(self, password: str) -> bool:
        """Authenticate with the device.

        Args:
            password: Device management password.

        Returns:
            True if authentication succeeded.
        """
        try:
            resp = requests.post(
                f"{self.base_url}/api/auth",
                json={"password": password},
                timeout=_REQUEST_TIMEOUT,
            )
            if resp.status_code == 200:
                data = resp.json()
                self.token = data.get("token")
                logger.info("Authenticated with %s", self.ip_address)
                return True
            logger.warning("Authentication failed for %s (HTTP %d)", self.ip_address, resp.status_code)
            return False
        except requests.RequestException as e:
            logger.error("Auth request failed for %s: %s", self.ip_address, e)
            return False

    def get_config(self) -> DeviceConfig | None:
        """Get device configuration (authentication required).

        Returns:
            DeviceConfig if successful, None otherwise.
        """
        try:
            resp = requests.get(
                f"{self.base_url}/api/config",
                headers=self._headers(),
                timeout=_REQUEST_TIMEOUT,
            )
            resp.raise_for_status()
            return DeviceConfig(**resp.json())
        except requests.RequestException as e:
            logger.error("Failed to get config from %s: %s", self.ip_address, e)
            return None

    def update_config(self, config: dict[str, Any]) -> bool:
        """Update device configuration (authentication required).

        Args:
            config: Dict of configuration values to update.

        Returns:
            True if update succeeded.
        """
        try:
            resp = requests.put(
                f"{self.base_url}/api/config",
                json=config,
                headers=self._headers(),
                timeout=_REQUEST_TIMEOUT,
            )
            return resp.status_code == 200
        except requests.RequestException as e:
            logger.error("Failed to update config on %s: %s", self.ip_address, e)
            return False

    def restart(self) -> bool:
        """Restart the device (authentication required).

        Returns:
            True if restart command accepted.
        """
        try:
            resp = requests.post(
                f"{self.base_url}/api/restart",
                headers=self._headers(),
                timeout=_REQUEST_TIMEOUT,
            )
            return resp.status_code == 200
        except requests.RequestException as e:
            logger.error("Failed to restart %s: %s", self.ip_address, e)
            return False

    def to_status(self, data: dict[str, Any]) -> DeviceStatus:
        """Convert raw status dict to a DeviceStatus model.

        Args:
            data: Raw JSON dict from ``/api/status``.

        Returns:
            Populated DeviceStatus model.
        """
        return DeviceStatus(
            device_id=data.get("device_id", "unknown"),
            ip_address=self.ip_address,
            firmware_version=data.get("firmware_version", ""),
            uptime_seconds=data.get("uptime_s", 0),
            hashrate_khs=data.get("hashrate_khs", 0.0),
            shares_accepted=data.get("shares_accepted", 0),
            shares_rejected=data.get("shares_rejected", 0),
            pool_connected=data.get("pool_connected", False),
            free_heap=data.get("free_heap", 0),
            authenticated=self.token is not None,
        )
