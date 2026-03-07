"""Flask application factory for PDQManager web server."""

from __future__ import annotations

import logging

from flask import Flask
from flask_cors import CORS

from pdqmanager.api import api_bp
from pdqmanager.manager import DeviceManager

logger = logging.getLogger(__name__)


def create_app(device_manager: DeviceManager | None = None) -> Flask:
    """Create and configure the Flask application.

    Args:
        device_manager: Optional pre-configured DeviceManager instance.
            If None, a new one is created.

    Returns:
        Configured Flask application.
    """
    app = Flask(
        __name__,
        template_folder="templates",
        static_folder="static",
    )

    CORS(app)

    if device_manager is None:
        device_manager = DeviceManager()

    app.config["device_manager"] = device_manager

    # Register API blueprint
    app.register_blueprint(api_bp)

    # Register page routes
    _register_page_routes(app)

    return app


def _register_page_routes(app: Flask) -> None:
    """Register HTML page routes on the Flask app."""
    from flask import render_template

    @app.route("/")
    def index() -> str:
        return render_template("index.html")

    @app.route("/device/<device_id>")
    def device_page(device_id: str) -> str:
        return render_template("device.html", device_id=device_id)

    @app.route("/settings")
    def settings_page() -> str:
        return render_template("settings.html")
