"""Tests for CLI interface."""

from __future__ import annotations

from unittest.mock import patch

import pytest
from click.testing import CliRunner

from pdqflasher.cli import main


class TestCLI:
    """Tests for pdqflasher CLI commands."""

    @pytest.fixture()
    def runner(self) -> CliRunner:
        """CLI test runner."""
        return CliRunner()

    def test_version_shows_version(self, runner: CliRunner) -> None:
        """--version flag shows version info."""
        result = runner.invoke(main, ["--version"])

        assert result.exit_code == 0
        assert "pdqflasher" in result.output.lower()

    def test_flash_no_port_no_device_shows_error(self, runner: CliRunner) -> None:
        """flash command without --port and no device shows error."""
        with patch("pdqflasher.cli.detect_port", return_value=None):
            result = runner.invoke(main, ["flash"])

        assert result.exit_code != 0
        assert "No ESP32 device detected" in result.output

    def test_detect_no_device_shows_error(self, runner: CliRunner) -> None:
        """detect command with no device connected shows error."""
        with patch("pdqflasher.cli.detect_port", return_value=None):
            result = runner.invoke(main, ["detect"])

        assert result.exit_code != 0
        assert "No ESP32 device detected" in result.output

    def test_help_shows_commands(self, runner: CliRunner) -> None:
        """--help lists available commands."""
        result = runner.invoke(main, ["--help"])

        assert result.exit_code == 0
        assert "flash" in result.output
        assert "detect" in result.output
        assert "erase" in result.output

    def test_flash_help_shows_options(self, runner: CliRunner) -> None:
        """flash --help lists available options."""
        result = runner.invoke(main, ["flash", "--help"])

        assert result.exit_code == 0
        assert "--board" in result.output
        assert "--port" in result.output
        assert "--binary" in result.output
        assert "--verify" in result.output
        assert "--erase" in result.output
