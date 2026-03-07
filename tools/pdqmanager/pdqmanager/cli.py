"""CLI interface for PDQManager."""

from __future__ import annotations

import json
import logging
import webbrowser

import click
from rich.console import Console

from pdqmanager import __version__
from pdqmanager.app import create_app
from pdqmanager.manager import DeviceManager

console = Console()
logger = logging.getLogger(__name__)


@click.group(invoke_without_command=True)
@click.version_option(version=__version__, prog_name="pdqmanager")
@click.option("--port", default=5000, show_default=True, help="Web server port")
@click.option("--no-browser", is_flag=True, help="Don't open browser automatically")
@click.pass_context
def main(ctx: click.Context, port: int, no_browser: bool) -> None:
    """PDQManager - Fleet management tool for PDQminer devices."""
    if ctx.invoked_subcommand is not None:
        return

    dm = DeviceManager()
    dm.start_discovery()

    app = create_app(device_manager=dm)

    url = f"http://127.0.0.1:{port}"
    console.print(f"[bold green]PDQManager v{__version__}[/bold green]")
    console.print(f"[green]Dashboard:[/green] {url}")
    console.print("[dim]Press Ctrl+C to stop[/dim]\n")

    if not no_browser:
        webbrowser.open(url)

    try:
        app.run(host="127.0.0.1", port=port, debug=False)
    except KeyboardInterrupt:
        pass
    finally:
        dm.stop_discovery()
        console.print("\n[yellow]PDQManager stopped.[/yellow]")


@main.command()
def scan() -> None:
    """Scan network for PDQminer devices (no web server)."""
    import time

    dm = DeviceManager()
    dm.start_discovery()

    console.print("[yellow]Scanning for PDQminer devices (5 seconds)...[/yellow]")
    time.sleep(5)
    dm.stop_discovery()

    devices = list(dm._discovery.devices.values()) if dm._discovery else []

    if not devices:
        console.print("[red]No devices found.[/red]")
        return

    console.print(f"\n[green]Found {len(devices)} device(s):[/green]\n")
    for dev in devices:
        console.print(f"  {dev.device_id}  {dev.ip_address}:{dev.port}  "
                       f"v{dev.version}  ({dev.hardware})")


@main.command()
@click.option("--format", "fmt", type=click.Choice(["json", "csv"]), default="json")
@click.option("--output", "-o", type=click.Path(), help="Output file path")
def export(fmt: str, output: str | None) -> None:
    """Export miner data to file."""
    import time

    dm = DeviceManager()
    dm.start_discovery()

    console.print("[yellow]Discovering devices (5 seconds)...[/yellow]")
    time.sleep(5)

    dm.refresh_all()
    all_status = dm.get_all_status()
    dm.stop_discovery()

    if not all_status:
        console.print("[red]No devices found to export.[/red]")
        return

    if fmt == "json":
        content = json.dumps(all_status, indent=2, default=str)
    else:
        # CSV format — use csv module for proper quoting/escaping
        import csv
        import io

        if not all_status:
            return
        headers = list(all_status[0].keys())
        buf = io.StringIO()
        writer = csv.writer(buf)
        writer.writerow(headers)
        for device in all_status:
            writer.writerow([str(device.get(h, "")) for h in headers])
        content = buf.getvalue()

    if output:
        with open(output, "w") as f:
            f.write(content)
        console.print(f"[green]Exported {len(all_status)} device(s) to {output}[/green]")
    else:
        console.print(content)
