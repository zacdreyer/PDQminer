"""Shared pytest fixtures for PDQManager tests."""

from __future__ import annotations

import pytest

from pdqmanager.app import create_app
from pdqmanager.manager import DeviceManager


@pytest.fixture()
def device_manager() -> DeviceManager:
    """Create a fresh DeviceManager for testing."""
    return DeviceManager()


@pytest.fixture()
def app(device_manager: DeviceManager):
    """Create a Flask test app."""
    app = create_app(device_manager=device_manager)
    app.config["TESTING"] = True
    return app


@pytest.fixture()
def client(app):
    """Create a Flask test client."""
    return app.test_client()
