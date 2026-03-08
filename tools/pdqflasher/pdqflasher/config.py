"""Board configurations for PDQminer devices."""

from __future__ import annotations

from dataclasses import dataclass

# Known USB-to-serial chip identifiers (VID, PID)
ESP32_USB_DEVICES: list[tuple[int, int]] = [
    (0x10C4, 0xEA60),  # Silicon Labs CP210x
    (0x1A86, 0x7523),  # WCH CH340/CH341
    (0x0403, 0x6001),  # FTDI FT232
    (0x303A, 0x1001),  # Espressif USB JTAG/serial (ESP32-S3/C3)
]


@dataclass(frozen=True)
class BoardConfig:
    """Configuration for a specific board variant."""

    chip: str
    flash_size: str
    flash_mode: str
    flash_freq: str
    firmware: str
    description: str


BOARD_CONFIGS: dict[str, BoardConfig] = {
    "cyd_ili9341": BoardConfig(
        chip="esp32",
        flash_size="4MB",
        flash_mode="dio",
        flash_freq="40m",
        firmware="pdqminer_cyd_ili9341.bin",
        description="ESP32-2432S028R with ILI9341 display",
    ),
    "cyd_st7789": BoardConfig(
        chip="esp32",
        flash_size="4MB",
        flash_mode="dio",
        flash_freq="40m",
        firmware="pdqminer_cyd_st7789.bin",
        description="ESP32-2432S028R with ST7789 display",
    ),
}


def get_board_config(board: str) -> BoardConfig:
    """Get configuration for a board variant.

    Args:
        board: Board name (e.g. ``cyd_ili9341``).

    Returns:
        Matching BoardConfig.

    Raises:
        KeyError: If the board name is unknown.
    """
    if board not in BOARD_CONFIGS:
        raise KeyError(f"Unknown board: {board}. Available: {', '.join(BOARD_CONFIGS)}")
    return BOARD_CONFIGS[board]
