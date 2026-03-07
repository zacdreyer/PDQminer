"""Unit tests for device communication client."""

from __future__ import annotations

import responses

from pdqmanager.device import DeviceClient
from pdqmanager.models import DeviceConfig


class TestDeviceClient:
    """Tests for DeviceClient HTTP communication."""

    @responses.activate
    def test_get_status_success(self) -> None:
        """Successful status request returns parsed JSON."""
        responses.add(
            responses.GET,
            "http://192.168.1.100/api/status",
            json={"device_id": "PDQ_ABC", "hashrate_khs": 520.5, "uptime_s": 3600},
            status=200,
        )
        client = DeviceClient("192.168.1.100")
        status = client.get_status()

        assert status["device_id"] == "PDQ_ABC"
        assert status["hashrate_khs"] == 520.5

    @responses.activate
    def test_get_info_success(self) -> None:
        """Successful info request returns parsed JSON."""
        responses.add(
            responses.GET,
            "http://192.168.1.100/api/info",
            json={"firmware_version": "1.0.0", "mac": "AA:BB:CC:DD:EE:FF"},
            status=200,
        )
        client = DeviceClient("192.168.1.100")
        info = client.get_info()

        assert info["firmware_version"] == "1.0.0"

    @responses.activate
    def test_authenticate_success(self) -> None:
        """Successful authentication stores token."""
        responses.add(
            responses.POST,
            "http://192.168.1.100/api/auth",
            json={"token": "abc123", "expires_in": 3600},
            status=200,
        )
        client = DeviceClient("192.168.1.100")
        result = client.authenticate("MyPassword123")

        assert result is True
        assert client.token == "abc123"

    @responses.activate
    def test_authenticate_wrong_password(self) -> None:
        """Failed authentication returns False and no token."""
        responses.add(
            responses.POST,
            "http://192.168.1.100/api/auth",
            json={"error": "unauthorized"},
            status=401,
        )
        client = DeviceClient("192.168.1.100")
        result = client.authenticate("WrongPassword")

        assert result is False
        assert client.token is None

    @responses.activate
    def test_get_config_authenticated(self) -> None:
        """Config request with valid token returns DeviceConfig."""
        responses.add(
            responses.GET,
            "http://192.168.1.100/api/config",
            json={
                "wifi_ssid": "MyNetwork",
                "pool1_host": "pool.example.com",
                "pool1_port": 3333,
                "wallet": "bc1qtest",
                "worker": "miner1",
            },
            status=200,
        )
        client = DeviceClient("192.168.1.100")
        client.token = "abc123"
        config = client.get_config()

        assert isinstance(config, DeviceConfig)
        assert config.pool1_host == "pool.example.com"
        assert config.wallet == "bc1qtest"

    @responses.activate
    def test_update_config_success(self) -> None:
        """Config update with valid token returns True."""
        responses.add(
            responses.PUT,
            "http://192.168.1.100/api/config",
            json={"status": "ok"},
            status=200,
        )
        client = DeviceClient("192.168.1.100")
        client.token = "abc123"
        result = client.update_config({"pool1_host": "newpool.com"})

        assert result is True

    @responses.activate
    def test_restart_success(self) -> None:
        """Restart request returns True on success."""
        responses.add(
            responses.POST,
            "http://192.168.1.100/api/restart",
            json={"status": "restarting"},
            status=200,
        )
        client = DeviceClient("192.168.1.100")
        client.token = "abc123"
        result = client.restart()

        assert result is True

    def test_to_status_converts_dict(self) -> None:
        """to_status converts raw dict to DeviceStatus model."""
        client = DeviceClient("192.168.1.100")
        data = {
            "device_id": "PDQ_ABC",
            "hashrate_khs": 520.5,
            "uptime_s": 7200,
            "shares_accepted": 42,
            "shares_rejected": 1,
            "pool_connected": True,
            "free_heap": 45000,
        }
        status = client.to_status(data)

        assert status.device_id == "PDQ_ABC"
        assert status.hashrate_khs == 520.5
        assert status.ip_address == "192.168.1.100"

    def test_base_url_without_port_80(self) -> None:
        """Default port 80 produces clean URL without port."""
        client = DeviceClient("192.168.1.100", port=80)
        assert client.base_url == "http://192.168.1.100"

    def test_base_url_with_custom_port(self) -> None:
        """Custom port is included in base URL."""
        client = DeviceClient("192.168.1.100", port=8080)
        assert client.base_url == "http://192.168.1.100:8080"
