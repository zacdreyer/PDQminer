"""Shared pytest fixtures for PDQFlasher tests."""

from __future__ import annotations

from pathlib import Path

import pytest


@pytest.fixture()
def firmware_bin(tmp_path: Path) -> Path:
    """Create a dummy firmware binary for testing."""
    fw = tmp_path / "test_firmware.bin"
    fw.write_bytes(b"\x00" * 1024)
    return fw
