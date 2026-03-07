"""Unit tests for mDNS device discovery."""

from __future__ import annotations

from unittest.mock import Mock, patch

import pytest

from pdqmanager.discovery import PDQMinerDiscovery
from pdqmanager.models import DiscoveredDevice


class TestPDQMinerDiscovery:
    """Tests for PDQMinerDiscovery class."""

    def test_service_type_is_correct(self) -> None:
        """Service type matches mDNS spec."""
        discovery = PDQMinerDiscovery(on_device_found=Mock())
        assert discovery.SERVICE_TYPE == "_pdqminer._tcp.local."

    def test_add_service_stores_device(self) -> None:
        """Discovered device is stored and callback is invoked."""
        callback = Mock()
        discovery = PDQMinerDiscovery(on_device_found=callback)

        mock_info = Mock()
        mock_info.addresses = [bytes([192, 168, 1, 100])]
        mock_info.port = 80
        mock_info.properties = {
            b"id": b"PDQ_ABC",
            b"version": b"1.0.0",
            b"hardware": b"ESP32",
        }

        with patch.object(discovery.zeroconf, "get_service_info", return_value=mock_info):
            discovery.add_service(
                discovery.zeroconf, "_pdqminer._tcp.local.", "PDQ_ABC._pdqminer._tcp.local."
            )

        assert "PDQ_ABC" in discovery.devices
        callback.assert_called_once()
        device = callback.call_args[0][0]
        assert isinstance(device, DiscoveredDevice)
        assert device.ip_address == "192.168.1.100"

    def test_add_service_no_info_ignored(self) -> None:
        """Service with no info is silently ignored."""
        callback = Mock()
        discovery = PDQMinerDiscovery(on_device_found=callback)

        with patch.object(discovery.zeroconf, "get_service_info", return_value=None):
            discovery.add_service(
                discovery.zeroconf, "_pdqminer._tcp.local.", "PDQ_ABC._pdqminer._tcp.local."
            )

        assert len(discovery.devices) == 0
        callback.assert_not_called()

    def test_remove_service_removes_device(self) -> None:
        """Removed service is deleted from device list."""
        callback = Mock()
        discovery = PDQMinerDiscovery(on_device_found=callback)
        discovery.devices["PDQ_ABC"] = DiscoveredDevice(
            device_id="PDQ_ABC", ip_address="192.168.1.100"
        )

        discovery.remove_service(
            discovery.zeroconf, "_pdqminer._tcp.local.", "PDQ_ABC._pdqminer._tcp.local."
        )

        assert "PDQ_ABC" not in discovery.devices

    def test_remove_service_unknown_device_noop(self) -> None:
        """Removing an unknown device does nothing."""
        discovery = PDQMinerDiscovery(on_device_found=Mock())

        # Should not raise
        discovery.remove_service(
            discovery.zeroconf, "_pdqminer._tcp.local.", "PDQ_UNKNOWN._pdqminer._tcp.local."
        )

    def test_update_service_refreshes_device(self) -> None:
        """Updated service re-processes the device."""
        callback = Mock()
        discovery = PDQMinerDiscovery(on_device_found=callback)

        mock_info = Mock()
        mock_info.addresses = [bytes([192, 168, 1, 101])]
        mock_info.port = 80
        mock_info.properties = {b"id": b"PDQ_XYZ", b"version": b"1.1.0", b"hardware": b"ESP32"}

        with patch.object(discovery.zeroconf, "get_service_info", return_value=mock_info):
            discovery.update_service(
                discovery.zeroconf, "_pdqminer._tcp.local.", "PDQ_XYZ._pdqminer._tcp.local."
            )

        assert "PDQ_XYZ" in discovery.devices
