"""Unit tests for configuration module."""

from __future__ import annotations

import pytest

from pdqflasher.config import BOARD_CONFIGS, BoardConfig, get_board_config


class TestBoardConfigs:
    """Tests for board configuration data."""

    def test_cyd_ili9341_config_exists(self) -> None:
        """CYD ILI9341 board configuration is defined."""
        assert "cyd_ili9341" in BOARD_CONFIGS

    def test_cyd_st7789_config_exists(self) -> None:
        """CYD ST7789 board configuration is defined."""
        assert "cyd_st7789" in BOARD_CONFIGS

    @pytest.mark.parametrize("board", ["cyd_ili9341", "cyd_st7789"])
    def test_config_has_required_fields(self, board: str) -> None:
        """All boards have required configuration fields."""
        config = BOARD_CONFIGS[board]
        assert isinstance(config, BoardConfig)
        assert config.chip
        assert config.flash_size
        assert config.flash_mode
        assert config.flash_freq
        assert config.firmware

    @pytest.mark.parametrize("board", ["cyd_ili9341", "cyd_st7789"])
    def test_config_chip_is_esp32(self, board: str) -> None:
        """All CYD boards use ESP32 chip."""
        assert BOARD_CONFIGS[board].chip == "esp32"

    @pytest.mark.parametrize("board", ["cyd_ili9341", "cyd_st7789"])
    def test_config_flash_size_is_4mb(self, board: str) -> None:
        """All CYD boards have 4MB flash."""
        assert BOARD_CONFIGS[board].flash_size == "4MB"

    def test_firmware_names_differ(self) -> None:
        """Each board has a unique firmware binary name."""
        firmwares = [cfg.firmware for cfg in BOARD_CONFIGS.values()]
        assert len(firmwares) == len(set(firmwares))


class TestGetBoardConfig:
    """Tests for get_board_config function."""

    def test_valid_board_returns_config(self) -> None:
        """Valid board name returns BoardConfig."""
        config = get_board_config("cyd_ili9341")

        assert isinstance(config, BoardConfig)
        assert config.chip == "esp32"

    def test_invalid_board_raises_keyerror(self) -> None:
        """Invalid board name raises KeyError."""
        with pytest.raises(KeyError):
            get_board_config("nonexistent_board")
