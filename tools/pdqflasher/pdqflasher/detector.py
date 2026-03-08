"""Board and serial port detection for ESP32 devices."""

from __future__ import annotations

import logging
from dataclasses import dataclass

from serial.tools import list_ports

from pdqflasher.config import ESP32_USB_DEVICES

logger = logging.getLogger(__name__)


@dataclass
class BoardInfo:
    """Detected board information."""

    chip: str
    mac: str
    port: str


def detect_port() -> str | None:
    """Auto-detect a serial port with an ESP32 device connected.

    Scans available serial ports and matches against known ESP32
    USB-to-serial chip vendor/product IDs.

    Returns:
        Serial port path (e.g. ``/dev/ttyUSB0``), or ``None`` if no device found.
    """
    known_pairs = set(ESP32_USB_DEVICES)

    for port_info in list_ports.comports():
        if port_info.vid is None:
            continue
        if (port_info.vid, port_info.pid) in known_pairs:
            logger.info("Detected ESP32 device on %s (%s)", port_info.device, port_info.description)
            return port_info.device

    return None


def detect_board(port: str) -> BoardInfo:
    """Detect ESP32 board type connected to the given port.

    Uses esptool to read chip ID and MAC address.

    Args:
        port: Serial port path.

    Returns:
        BoardInfo with chip type and MAC address.

    Raises:
        ConnectionError: If the device cannot be reached.
    """
    try:
        from esptool.cmds import detect_chip

        esp = detect_chip(port)
        try:
            chip_name = esp.CHIP_NAME
            mac_bytes = esp.read_mac()
            mac_str = ":".join(f"{b:02X}" for b in mac_bytes)
            return BoardInfo(chip=chip_name, mac=mac_str, port=port)
        finally:
            try:
                esp._port.close()
            except Exception:
                pass
    except Exception as e:
        raise ConnectionError(f"Failed to detect board on {port}: {e}") from e


def get_chip_info(port: str) -> dict[str, str]:
    """Get detailed chip information.

    Args:
        port: Serial port path.

    Returns:
        Dict with chip details (chip, mac, port).

    Raises:
        ConnectionError: If the device cannot be reached.
    """
    board = detect_board(port)
    return {"chip": board.chip, "mac": board.mac, "port": board.port}
