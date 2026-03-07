"""Unit tests for PDQManager REST API endpoints."""

from __future__ import annotations

from unittest.mock import patch


class TestAPIDevices:
    """Tests for /api/devices endpoints."""

    def test_get_devices_empty(self, client) -> None:
        """Empty fleet returns empty list."""
        response = client.get("/api/devices")

        assert response.status_code == 200
        assert response.json == []

    def test_get_devices_with_data(self, client, device_manager) -> None:
        """Fleet with cached status returns device list."""
        device_manager._status_cache["PDQ_ABC"] = {
            "device_id": "PDQ_ABC",
            "hashrate_khs": 520.5,
        }

        response = client.get("/api/devices")

        assert response.status_code == 200
        assert len(response.json) == 1
        assert response.json[0]["device_id"] == "PDQ_ABC"

    def test_get_device_not_found(self, client) -> None:
        """Unknown device returns 404."""
        response = client.get("/api/devices/UNKNOWN")

        assert response.status_code == 404

    def test_get_device_found(self, client, device_manager) -> None:
        """Known device returns its status."""
        device_manager._status_cache["PDQ_ABC"] = {
            "device_id": "PDQ_ABC",
            "hashrate_khs": 520.5,
        }

        response = client.get("/api/devices/PDQ_ABC")

        assert response.status_code == 200
        assert response.json["hashrate_khs"] == 520.5


class TestAPIAuth:
    """Tests for /api/devices/<id>/auth endpoint."""

    def test_auth_missing_password(self, client) -> None:
        """Auth without password returns 400."""
        response = client.post(
            "/api/devices/PDQ_ABC/auth",
            json={},
            content_type="application/json",
        )

        assert response.status_code == 400

    def test_auth_no_body(self, client) -> None:
        """Auth without JSON body returns error."""
        response = client.post("/api/devices/PDQ_ABC/auth")

        assert response.status_code in (400, 415)


class TestAPIStats:
    """Tests for /api/stats endpoint."""

    def test_get_stats_empty(self, client) -> None:
        """Empty fleet returns zero stats."""
        response = client.get("/api/stats")

        assert response.status_code == 200
        data = response.json
        assert data["total_devices"] == 0
        assert data["total_hashrate_khs"] == 0.0

    def test_get_stats_aggregates(self, client, device_manager) -> None:
        """Stats aggregate across multiple devices."""
        device_manager._status_cache["PDQ_A"] = {
            "hashrate_khs": 500,
            "shares_accepted": 10,
            "pool_connected": True,
        }
        device_manager._status_cache["PDQ_B"] = {
            "hashrate_khs": 520,
            "shares_accepted": 15,
            "pool_connected": True,
        }

        response = client.get("/api/stats")

        data = response.json
        assert data["total_devices"] == 2
        assert data["online_devices"] == 2
        assert data["total_hashrate_khs"] == 1020.0
        assert data["total_shares_accepted"] == 25
        assert data["avg_hashrate_khs"] == 510.0


class TestAPIScan:
    """Tests for /api/scan endpoint."""

    def test_scan_returns_count(self, client) -> None:
        """Scan endpoint returns device count."""
        response = client.post("/api/scan")

        assert response.status_code == 200
        assert "devices" in response.json


class TestAPIConfig:
    """Tests for /api/devices/<id>/config endpoints."""

    def test_get_config_not_authenticated(self, client) -> None:
        """Config request without auth returns 404."""
        response = client.get("/api/devices/PDQ_ABC/config")

        assert response.status_code == 404

    def test_update_config_no_data(self, client) -> None:
        """Config update without data returns 400."""
        response = client.put(
            "/api/devices/PDQ_ABC/config",
            content_type="application/json",
        )

        assert response.status_code == 400


class TestAPIRestart:
    """Tests for /api/devices/<id>/restart endpoint."""

    def test_restart_no_client(self, client) -> None:
        """Restart unknown device returns 500."""
        response = client.post("/api/devices/UNKNOWN/restart")

        assert response.status_code == 500
