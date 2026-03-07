"""Unit tests for flash operations module."""

from __future__ import annotations

from pathlib import Path

import pytest
from unittest.mock import patch

from pdqflasher.flasher import flash_firmware, verify_firmware, erase_flash


class TestFlashFirmware:
    """Tests for firmware flashing."""

    @patch("pdqflasher.flasher.esptool")
    def test_flash_firmware_valid_binary_succeeds(
        self, mock_esptool: object, tmp_path: Path
    ) -> None:
        """Flashing valid binary completes successfully."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b"\x00" * 1024)

        result = flash_firmware(
            port="/dev/ttyUSB0",
            firmware_path=str(firmware),
            board="cyd_ili9341",
        )

        assert result["success"] is True

    def test_flash_firmware_missing_file_raises(self) -> None:
        """Flashing non-existent file raises FileNotFoundError."""
        with pytest.raises(FileNotFoundError):
            flash_firmware(
                port="/dev/ttyUSB0",
                firmware_path="/nonexistent/firmware.bin",
                board="cyd_ili9341",
            )

    @patch("pdqflasher.flasher.esptool")
    def test_flash_firmware_invalid_board_raises(
        self, mock_esptool: object, tmp_path: Path
    ) -> None:
        """Flashing with invalid board name raises ValueError."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b"\x00" * 1024)

        with pytest.raises(ValueError, match="Unknown board"):
            flash_firmware(
                port="/dev/ttyUSB0",
                firmware_path=str(firmware),
                board="invalid_board",
            )

    @patch("pdqflasher.flasher.esptool")
    def test_flash_firmware_with_verify_flag(
        self, mock_esptool: object, tmp_path: Path
    ) -> None:
        """Verify flag is passed to esptool."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b"\x00" * 1024)

        result = flash_firmware(
            port="/dev/ttyUSB0",
            firmware_path=str(firmware),
            board="cyd_ili9341",
            verify=True,
        )

        assert result["success"] is True
        call_args = mock_esptool.main.call_args[0][0]
        assert "--verify" in call_args

    @patch("pdqflasher.flasher.esptool")
    def test_flash_firmware_esptool_exit_zero_succeeds(
        self, mock_esptool: object, tmp_path: Path
    ) -> None:
        """esptool exiting with code 0 is treated as success."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b"\x00" * 1024)
        mock_esptool.main.side_effect = SystemExit(0)

        result = flash_firmware(
            port="/dev/ttyUSB0",
            firmware_path=str(firmware),
            board="cyd_ili9341",
        )

        assert result["success"] is True

    @patch("pdqflasher.flasher.esptool")
    def test_flash_firmware_esptool_failure_returns_error(
        self, mock_esptool: object, tmp_path: Path
    ) -> None:
        """esptool failure returns error dict."""
        firmware = tmp_path / "test.bin"
        firmware.write_bytes(b"\x00" * 1024)
        mock_esptool.main.side_effect = SystemExit(1)

        result = flash_firmware(
            port="/dev/ttyUSB0",
            firmware_path=str(firmware),
            board="cyd_ili9341",
        )

        assert result["success"] is False
        assert "error" in result


class TestVerifyFirmware:
    """Tests for firmware verification."""

    @patch("pdqflasher.flasher.esptool")
    def test_verify_firmware_success(self, mock_esptool: object) -> None:
        """Verification passing returns True."""
        mock_esptool.main.return_value = None

        result = verify_firmware("/dev/ttyUSB0", expected_checksum="abc123")

        assert result is True

    @patch("pdqflasher.flasher.esptool")
    def test_verify_firmware_failure(self, mock_esptool: object) -> None:
        """Verification failing returns False."""
        mock_esptool.main.side_effect = Exception("Checksum mismatch")

        result = verify_firmware("/dev/ttyUSB0", expected_checksum="abc123")

        assert result is False


class TestEraseFlash:
    """Tests for flash erase."""

    @patch("pdqflasher.flasher.esptool")
    def test_erase_flash_success(self, mock_esptool: object) -> None:
        """Successful erase returns success dict."""
        mock_esptool.main.return_value = None

        result = erase_flash("/dev/ttyUSB0")

        assert result["success"] is True

    @patch("pdqflasher.flasher.esptool")
    def test_erase_flash_failure(self, mock_esptool: object) -> None:
        """Failed erase returns error dict."""
        mock_esptool.main.side_effect = Exception("Permission denied")

        result = erase_flash("/dev/ttyUSB0")

        assert result["success"] is False
        assert "Permission denied" in str(result["error"])
