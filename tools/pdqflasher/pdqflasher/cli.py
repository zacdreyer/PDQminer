"""CLI interface for PDQFlasher."""

from __future__ import annotations

import click
from rich.console import Console
from rich.table import Table

from pdqflasher import __version__
from pdqflasher.config import BOARD_CONFIGS
from pdqflasher.detector import detect_board, detect_port
from pdqflasher.flasher import erase_flash, flash_firmware

console = Console()


@click.group()
@click.version_option(version=__version__, prog_name="pdqflasher")
def main() -> None:
    """PDQFlasher - Firmware flashing tool for PDQminer devices."""


@main.command()
@click.option("--board", type=click.Choice(list(BOARD_CONFIGS.keys())), help="Board type")
@click.option("--port", help="Serial port (auto-detect if omitted)")
@click.option(
    "--binary",
    "firmware_path",
    type=click.Path(exists=True),
    help="Firmware binary path",
)
@click.option("--verify", is_flag=True, help="Verify after flashing")
@click.option("--erase", is_flag=True, help="Erase flash before writing")
@click.option("--baud", default=460800, show_default=True, help="Serial baud rate")
def flash(
    board: str | None,
    port: str | None,
    firmware_path: str | None,
    verify: bool,
    erase: bool,
    baud: int,
) -> None:
    """Flash firmware to a PDQminer device."""
    if port is None:
        port = detect_port()
        if port is None:
            console.print("[red]No ESP32 device detected.[/red] Connect a device or use --port.")
            raise SystemExit(1)
        console.print(f"[green]Auto-detected port:[/green] {port}")

    if board is None:
        console.print("[yellow]Detecting board type...[/yellow]")
        try:
            info = detect_board(port)
            console.print(f"[green]Detected:[/green] {info.chip} (MAC: {info.mac})")
            board = "cyd_ili9341"
            console.print(f"[yellow]Using default board config:[/yellow] {board}")
        except ConnectionError as e:
            console.print(f"[red]Board detection failed:[/red] {e}")
            console.print("Use --board to specify the board type manually.")
            raise SystemExit(1)

    if firmware_path is None:
        config = BOARD_CONFIGS[board]
        firmware_path = config.firmware
        console.print(f"[yellow]Using default firmware:[/yellow] {firmware_path}")

    console.print(f"\n[bold]Flashing {firmware_path} → {port} (board={board})[/bold]\n")

    result = flash_firmware(
        port=port,
        firmware_path=firmware_path,
        board=board,
        verify=verify,
        erase=erase,
        baud=baud,
    )

    if result["success"]:
        console.print("\n[bold green]Flash complete![/bold green]")
    else:
        console.print(
            f"\n[bold red]Flash failed:[/bold red] {result.get('error', 'Unknown error')}"
        )
        raise SystemExit(1)


@main.command()
@click.option("--port", help="Serial port (auto-detect if omitted)")
def detect(port: str | None) -> None:
    """Detect connected ESP32 devices."""
    if port is None:
        port = detect_port()
        if port is None:
            console.print("[red]No ESP32 device detected.[/red]")
            raise SystemExit(1)

    console.print(f"[green]Found device on:[/green] {port}")

    try:
        info = detect_board(port)
        table = Table(title="Device Information")
        table.add_column("Property", style="cyan")
        table.add_column("Value", style="green")
        table.add_row("Port", info.port)
        table.add_row("Chip", info.chip)
        table.add_row("MAC Address", info.mac)
        console.print(table)
    except ConnectionError as e:
        console.print(f"[red]Detection failed:[/red] {e}")
        raise SystemExit(1)


@main.command()
@click.option("--port", required=True, help="Serial port")
@click.option("--baud", default=460800, show_default=True, help="Serial baud rate")
def erase(port: str, baud: int) -> None:
    """Erase flash memory on an ESP32 device."""
    console.print(f"[yellow]Erasing flash on {port}...[/yellow]")

    result = erase_flash(port, baud=baud)

    if result["success"]:
        console.print("[bold green]Flash erased successfully![/bold green]")
    else:
        console.print(f"[bold red]Erase failed:[/bold red] {result.get('error', 'Unknown error')}")
        raise SystemExit(1)
