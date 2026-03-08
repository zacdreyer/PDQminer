"""mDNS/UDP device discovery for PDQminer devices on the local network."""

from __future__ import annotations

import logging
import socket
from collections.abc import Callable

from zeroconf import ServiceBrowser, ServiceListener, Zeroconf

from pdqmanager.models import DiscoveredDevice

logger = logging.getLogger(__name__)


class PDQMinerDiscovery(ServiceListener):
    """Discover PDQminer devices via mDNS (``_pdqminer._tcp.local.``)."""

    SERVICE_TYPE = "_pdqminer._tcp.local."

    def __init__(self, on_device_found: Callable[[DiscoveredDevice], None]) -> None:
        self.devices: dict[str, DiscoveredDevice] = {}
        self.on_device_found = on_device_found
        self.zeroconf = Zeroconf()
        self.browser: ServiceBrowser | None = None

    def start(self) -> None:
        """Begin listening for mDNS advertisements."""
        logger.info("Starting mDNS discovery for %s", self.SERVICE_TYPE)
        self.browser = ServiceBrowser(self.zeroconf, self.SERVICE_TYPE, self)

    def stop(self) -> None:
        """Stop discovery and release resources."""
        if self.browser:
            self.browser.cancel()
        self.zeroconf.close()
        logger.info("mDNS discovery stopped")

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        """Handle a newly discovered mDNS service."""
        info = zc.get_service_info(type_, name)
        if info and info.addresses:
            ip = socket.inet_ntoa(info.addresses[0])
            txt: dict[str, str] = {}
            if info.properties:
                txt = {
                    k.decode("utf-8", errors="replace"): v.decode("utf-8", errors="replace")
                    for k, v in info.properties.items()
                    if isinstance(k, bytes) and isinstance(v, bytes)
                }

            device = DiscoveredDevice(
                device_id=txt.get("id", name.split(".")[0]),
                ip_address=ip,
                port=info.port or 80,
                version=txt.get("version", ""),
                hardware=txt.get("hardware", ""),
            )
            self.devices[device.device_id] = device
            logger.info("Discovered device: %s @ %s:%d", device.device_id, ip, device.port)
            self.on_device_found(device)

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        """Handle an updated mDNS service (re-discover)."""
        self.add_service(zc, type_, name)

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        """Handle a removed mDNS service."""
        device_id = name.split(".")[0]
        removed = self.devices.pop(device_id, None)
        if removed:
            logger.info("Device removed: %s", device_id)
