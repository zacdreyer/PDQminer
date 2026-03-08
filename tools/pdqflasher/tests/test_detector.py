"""Unit tests for board detection module."""

from __future__ import annotations

from unittest.mock import Mock, patch

import pytest

from pdqflasher.detector import detect_board, detect_port, get_chip_info


class TestDetectPort:
    """Tests for serial port detection."""

    @patch("pdqflasher.detector.list_ports.comports")
    def test_detect_port_single_esp32_returns_port(self, mock_comports: Mock) -> None:
        """When single ESP32 connected, returns its port."""
        mock_port = Mock()
        mock_port.device = "/dev/ttyUSB0"
        mock_port.vid = 0x10C4  # Silicon Labs CP210x
        mock_port.pid = 0xEA60
        mock_comports.return_value = [mock_port]

        result = detect_port()

        assert result == "/dev/ttyUSB0"

    @patch("pdqflasher.detector.list_ports.comports")
    def test_detect_port_no_devices_returns_none(self, mock_comports: Mock) -> None:
        """When no ESP32 devices connected, returns None."""
        mock_comports.return_value = []

        result = detect_port()

        assert result is None

    @patch("pdqflasher.detector.list_ports.comports")
    def test_detect_port_multiple_devices_returns_first(self, mock_comports: Mock) -> None:
        """When multiple ESP32 devices, returns first one."""
        mock_port1 = Mock(device="/dev/ttyUSB0", vid=0x10C4, pid=0xEA60)
        mock_port2 = Mock(device="/dev/ttyUSB1", vid=0x10C4, pid=0xEA60)
        mock_comports.return_value = [mock_port1, mock_port2]

        result = detect_port()

        assert result == "/dev/ttyUSB0"

    @patch("pdqflasher.detector.list_ports.comports")
    def test_detect_port_non_esp32_device_skipped(self, mock_comports: Mock) -> None:
        """Non-ESP32 USB devices are ignored."""
        mock_port = Mock(device="/dev/ttyUSB0", vid=0x1234, pid=0x5678)
        mock_comports.return_value = [mock_port]

        result = detect_port()

        assert result is None

    @patch("pdqflasher.detector.list_ports.comports")
    def test_detect_port_device_without_vid_skipped(self, mock_comports: Mock) -> None:
        """Devices without VID (e.g. Bluetooth serial) are skipped."""
        mock_port = Mock(device="/dev/ttyS0", vid=None, pid=None)
        mock_comports.return_value = [mock_port]

        result = detect_port()

        assert result is None

    @patch("pdqflasher.detector.list_ports.comports")
    def test_detect_port_ch340_device_detected(self, mock_comports: Mock) -> None:
        """CH340/CH341 USB-to-serial chips are detected."""
        mock_port = Mock(device="/dev/ttyUSB0", vid=0x1A86, pid=0x7523)
        mock_comports.return_value = [mock_port]

        result = detect_port()

        assert result == "/dev/ttyUSB0"


class TestDetectBoard:
    """Tests for board type detection."""

    @patch("esptool.cmds.detect_chip")
    def test_detect_board_esp32_returns_board_info(self, mock_detect: Mock) -> None:
        """When ESP32 detected, returns BoardInfo."""
        mock_esp = Mock()
        mock_esp.CHIP_NAME = "ESP32"
        mock_esp.read_mac.return_value = b"\xaa\xbb\xcc\xdd\xee\xff"
        mock_detect.return_value = mock_esp

        result = detect_board("/dev/ttyUSB0")

        assert result.chip == "ESP32"
        assert result.mac == "AA:BB:CC:DD:EE:FF"
        assert result.port == "/dev/ttyUSB0"

    @patch("esptool.cmds.detect_chip")
    def test_detect_board_connection_error_raises(self, mock_detect: Mock) -> None:
        """When connection fails, raises ConnectionError."""
        mock_detect.side_effect = Exception("Failed to connect")

        with pytest.raises(ConnectionError):
            detect_board("/dev/ttyUSB0")


class TestGetChipInfo:
    """Tests for get_chip_info helper."""

    @patch("esptool.cmds.detect_chip")
    def test_get_chip_info_returns_dict(self, mock_detect: Mock) -> None:
        """Returns dict with chip, mac, port keys."""
        mock_esp = Mock()
        mock_esp.CHIP_NAME = "ESP32"
        mock_esp.read_mac.return_value = b"\x01\x02\x03\x04\x05\x06"
        mock_detect.return_value = mock_esp

        result = get_chip_info("/dev/ttyUSB0")

        assert result["chip"] == "ESP32"
        assert result["mac"] == "01:02:03:04:05:06"
        assert result["port"] == "/dev/ttyUSB0"
