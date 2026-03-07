"""REST API endpoints for PDQManager web server."""

from __future__ import annotations

import logging
from typing import Any

from flask import Blueprint, jsonify, request

logger = logging.getLogger(__name__)

api_bp = Blueprint("api", __name__, url_prefix="/api")


def get_device_manager() -> Any:
    """Get the device manager from the Flask app context."""
    from flask import current_app

    return current_app.config["device_manager"]


@api_bp.route("/devices", methods=["GET"])
def get_devices() -> tuple[Any, int]:
    """List all discovered devices with their current status."""
    dm = get_device_manager()
    devices = dm.get_all_status()
    return jsonify(devices), 200


@api_bp.route("/devices/<device_id>", methods=["GET"])
def get_device(device_id: str) -> tuple[Any, int]:
    """Get status for a single device."""
    dm = get_device_manager()
    status = dm.get_device_status(device_id)
    if status is None:
        return jsonify({"error": "Device not found"}), 404
    return jsonify(status), 200


@api_bp.route("/devices/<device_id>/config", methods=["GET"])
def get_device_config(device_id: str) -> tuple[Any, int]:
    """Get configuration for a single device (requires prior auth)."""
    dm = get_device_manager()
    config = dm.get_device_config(device_id)
    if config is None:
        return jsonify({"error": "Device not found or not authenticated"}), 404
    return jsonify(config), 200


@api_bp.route("/devices/<device_id>/config", methods=["PUT"])
def update_device_config(device_id: str) -> tuple[Any, int]:
    """Update configuration for a single device."""
    dm = get_device_manager()
    data = request.get_json()
    if not data:
        return jsonify({"error": "No data provided"}), 400
    if dm.update_device_config(device_id, data):
        return jsonify({"status": "ok"}), 200
    return jsonify({"error": "Update failed"}), 500


@api_bp.route("/devices/<device_id>/auth", methods=["POST"])
def auth_device(device_id: str) -> tuple[Any, int]:
    """Authenticate with a device using its management password."""
    dm = get_device_manager()
    data = request.get_json()
    if not data or "password" not in data:
        return jsonify({"error": "Password required"}), 400
    if dm.authenticate_device(device_id, data["password"]):
        return jsonify({"status": "authenticated"}), 200
    return jsonify({"error": "Authentication failed"}), 401


@api_bp.route("/devices/<device_id>/restart", methods=["POST"])
def restart_device(device_id: str) -> tuple[Any, int]:
    """Restart a device (requires prior auth)."""
    dm = get_device_manager()
    if dm.restart_device(device_id):
        return jsonify({"status": "restarting"}), 200
    return jsonify({"error": "Restart failed"}), 500


@api_bp.route("/scan", methods=["POST"])
def trigger_scan() -> tuple[Any, int]:
    """Trigger a network scan for new devices."""
    dm = get_device_manager()
    dm.refresh_all()
    devices = dm.get_all_status()
    return jsonify({"status": "scan_complete", "devices": len(devices)}), 200


@api_bp.route("/stats", methods=["GET"])
def get_stats() -> tuple[Any, int]:
    """Get aggregate fleet statistics."""
    dm = get_device_manager()
    all_status = dm.get_all_status()

    online = [d for d in all_status if d.get("pool_connected", False)]
    total_hr = sum(d.get("hashrate_khs", 0) for d in all_status)
    total_shares = sum(d.get("shares_accepted", 0) for d in all_status)
    device_count = len(all_status)

    stats = {
        "total_devices": device_count,
        "online_devices": len(online),
        "total_hashrate_khs": round(total_hr, 1),
        "total_shares_accepted": total_shares,
        "avg_hashrate_khs": round(total_hr / device_count, 1) if device_count else 0.0,
    }
    return jsonify(stats), 200
