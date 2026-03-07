"""Firmware flashing operations using esptool."""

from __future__ import annotations

import logging
from pathlib import Path

import esptool

from pdqflasher.config import BOARD_CONFIGS, get_board_config

logger = logging.getLogger(__name__)


def flash_firmware(
    port: str,
    firmware_path: str,
    board: str,
    verify: bool = False,
    erase: bool = False,
    baud: int = 460800,
) -> dict[str, object]:
    """Flash firmware to an ESP32 device.

    Args:
        port: Serial port path.
        firmware_path: Path to the firmware binary file.
        board: Board configuration name (e.g. ``cyd_ili9341``).
        verify: Verify flash contents after writing.
        erase: Erase flash before writing.
        baud: Serial baud rate.

    Returns:
        Dict with ``success`` bool and optional ``error`` message.

    Raises:
        FileNotFoundError: If firmware binary does not exist.
        ValueError: If board name is unknown.
    """
    fw_path = Path(firmware_path)
    if not fw_path.is_file():
        raise FileNotFoundError(f"Firmware binary not found: {firmware_path}")

    if board not in BOARD_CONFIGS:
        raise ValueError(f"Unknown board: {board}. Available: {', '.join(BOARD_CONFIGS)}")

    config = get_board_config(board)

    if erase:
        logger.info("Erasing flash on %s", port)
        erase_flash(port, baud=baud)

    args = [
        "--chip",
        config.chip,
        "--port",
        port,
        "--baud",
        str(baud),
        "write_flash",
        "--flash_mode",
        config.flash_mode,
        "--flash_freq",
        config.flash_freq,
        "--flash_size",
        config.flash_size,
    ]

    if verify:
        args.append("--verify")

    args.extend(["0x0", str(fw_path)])

    logger.info("Flashing %s to %s (board=%s)", fw_path.name, port, board)

    try:
        esptool.main(args)
        return {"success": True}
    except SystemExit as e:
        if e.code == 0:
            return {"success": True}
        return {"success": False, "error": f"esptool exited with code {e.code}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def verify_firmware(port: str, firmware_path: str, baud: int = 460800) -> bool:
    """Verify flashed firmware matches the binary file on disk.

    Uses esptool's ``verify_flash`` command to read back flash contents
    and compare byte-by-byte against the original binary.

    Args:
        port: Serial port path.
        firmware_path: Path to the original firmware binary to verify against.
        baud: Serial baud rate.

    Returns:
        True if verification passes, False otherwise.

    Raises:
        FileNotFoundError: If firmware binary does not exist.
    """
    fw_path = Path(firmware_path)
    if not fw_path.is_file():
        raise FileNotFoundError(f"Firmware binary not found: {firmware_path}")

    try:
        args = [
            "--port",
            port,
            "--baud",
            str(baud),
            "verify_flash",
            "0x0",
            str(fw_path),
        ]
        esptool.main(args)
        return True
    except SystemExit as e:
        if e.code == 0:
            return True
        return False
    except Exception:
        return False


def erase_flash(port: str, baud: int = 460800) -> dict[str, object]:
    """Erase entire flash on an ESP32 device.

    Args:
        port: Serial port path.
        baud: Serial baud rate.

    Returns:
        Dict with ``success`` bool and optional ``error`` message.
    """
    logger.info("Erasing flash on %s", port)

    try:
        args = [
            "--port",
            port,
            "--baud",
            str(baud),
            "erase_flash",
        ]
        esptool.main(args)
        return {"success": True}
    except SystemExit as e:
        if e.code == 0:
            return {"success": True}
        return {"success": False, "error": f"esptool exited with code {e.code}"}
    except Exception as e:
        return {"success": False, "error": str(e)}
