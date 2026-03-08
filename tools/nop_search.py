#!/usr/bin/env python3
"""
tools/nop_search.py - Automated NOP binary search for PDQminer SHA256 engine.

For each NOP phase in the SHA256 HW mining pipeline, binary searches for the
minimum number of NOP.N instructions that produce correct SHA256d hashes.

Workflow per test:
  1. Modify NOP macro in sha256_engine.c
  2. Build with PlatformIO (incremental, ~4s)
  3. Flash with esptool (~5s)
  4. Read boot-time correctness test result via serial (~3s)

Usage:
  python3 tools/nop_search.py [--port PORT] [--safety N] [--phase NAME]
"""

import argparse
import os
import re
import subprocess
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial required. Install: pip3 install pyserial")
    sys.exit(1)

# ── Paths ─────────────────────────────────────────────────────────────
WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHA_FILE  = os.path.join(WORKSPACE, 'src', 'core', 'sha256_engine.c')
FIRMWARE  = os.path.join(WORKSPACE, '.pio', 'build', 'cyd_ili9341', 'firmware.bin')
ESPTOOL   = os.path.expanduser('~/.platformio/packages/tool-esptoolpy/esptool.py')

DEFAULT_PORT = '/dev/cu.usbserial-130'
FLASH_BAUD   = 460800
SERIAL_BAUD  = 115200

# NOP phases: (macro_name, default_value, search_lo, search_hi)
# Ordered by biggest potential savings first.
PHASES = [
    ('NOP_57', 57,  5, 57),   # Phase 3: CONTINUE → LOAD
    ('NOP_50', 50,  5, 50),   # Phase 5: START dbl-hash → LOAD final
    ('NOP_15', 15,  1, 15),   # Phase 6: LOAD final → hash check
    ('NOP_13', 15,  0, 15),   # Phase 1: block1 fill → CONTINUE (was 13, increased for Bswap32-free path)
    ('NOP_9',   9,  1,  9),   # Phase 4: LOAD → START dbl-hash
    ('NOP_8',   8,  0,  8),   # Phase 8: START next → nonce update
]


# ── NOP Macro Manipulation ───────────────────────────────────────────

def _gen_nop_body(count):
    """Generate the asm string body containing 'count' nop.n instructions."""
    if count == 0:
        return '""'
    PER_LINE = 10
    groups = []
    for i in range(0, count, PER_LINE):
        n = min(PER_LINE, count - i)
        groups.append('\\n'.join(['nop.n'] * n))
    if len(groups) == 1:
        return f'"{groups[0]}"'
    parts = []
    for i, g in enumerate(groups):
        if i < len(groups) - 1:
            parts.append(f'"{g}\\n"')
        else:
            parts.append(f'"{g}"')
    return ' \\\n    '.join(parts)


def set_nop_value(macro_name, count):
    """Rewrite a NOP macro definition with the given NOP count."""
    with open(SHA_FILE, 'r') as f:
        content = f.read()

    # Match: #define NOP_XX __asm__ volatile( ... ::: "memory")
    pattern = rf'#define {re.escape(macro_name)} __asm__ volatile\([^)]*::: "memory"\)'
    m = re.search(pattern, content)
    if not m:
        raise RuntimeError(f"Macro {macro_name} not found in {SHA_FILE}")

    body = _gen_nop_body(count)
    # Build replacement with raw string concatenation to preserve literal \n
    replacement = '#define ' + macro_name + ' __asm__ volatile( \\\n    ' + body + ' \\\n    ::: "memory")'

    # Use direct string splicing instead of re.sub to avoid \n interpretation
    content = content[:m.start()] + replacement + content[m.end():]

    with open(SHA_FILE, 'w') as f:
        f.write(content)


def get_nop_value(macro_name):
    """Read the current NOP count by counting nop.n occurrences in the macro."""
    with open(SHA_FILE, 'r') as f:
        content = f.read()
    pattern = rf'#define {re.escape(macro_name)} __asm__ volatile\(([^)]*)::: "memory"\)'
    m = re.search(pattern, content)
    if not m:
        raise RuntimeError(f"Macro {macro_name} not found")
    return m.group(1).count('nop.n')


# ── Build / Flash / Serial ───────────────────────────────────────────

def build():
    """Incremental PlatformIO build. Returns True on success."""
    proc = subprocess.run(
        ['pio', 'run', '-e', 'cyd_ili9341'],
        cwd=WORKSPACE,
        capture_output=True, text=True, timeout=120
    )
    if proc.returncode != 0:
        print(f"  Build stderr: {proc.stderr[-200:]}")
    return proc.returncode == 0


def flash_and_read(port, timeout=20):
    """Flash firmware, then immediately open serial and reset device to
    capture the full boot output including the mining loop correctness test.
    Uses --after no_reset so we can manually reset AFTER opening serial.
    Returns True (PASS), False (FAIL), or None (timeout/error).

    Oracle: [LOOP-TEST] Result — this exercises the ACTUAL mining hot loop
    (NOP-timed, multi-nonce, with branches/nonce-increment).  The boot test
    (NOP vs SW) can false-positive with aggressive NOP values."""

    # Flash without auto-reset
    for attempt in range(3):
        proc = subprocess.run(
            ['python3', ESPTOOL, '--port', port, '--baud', str(FLASH_BAUD),
             '--chip', 'esp32', '--after', 'no_reset',
             'write_flash', '0x10000', FIRMWARE],
            capture_output=True, text=True, timeout=30
        )
        if proc.returncode == 0:
            break
        print(f"  Flash attempt {attempt+1} failed, retrying...")
        time.sleep(1)
    else:
        print("  FLASH FAILED after 3 attempts")
        return None

    # Open serial port FIRST, then manually reset the device
    time.sleep(0.2)
    try:
        ser = serial.Serial(port, SERIAL_BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"  Serial open error: {e}")
        return None

    # Hardware reset via RTS toggle (EN pin on ESP32 dev boards)
    ser.setDTR(False)
    ser.setRTS(True)   # Pull EN low (reset)
    time.sleep(0.1)
    ser.setRTS(False)   # Release EN (boot)
    ser.setDTR(False)
    time.sleep(0.1)
    ser.reset_input_buffer()

    # Read serial until we see the mining loop test result
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('utf-8', errors='ignore').strip()
        except Exception:
            continue
        if '[LOOP-TEST] Result: PASS' in line:
            ser.close()
            return True
        if '[LOOP-TEST] Result: FAIL' in line:
            ser.close()
            return False
        if 'CRITICAL' in line and 'halting' in line:
            ser.close()
            return False
    ser.close()
    return None


def test_config(port):
    """Build, flash, read test result. Returns True/False/None."""
    if not build():
        return None
    result = flash_and_read(port)
    if result is None:
        # Retry once (sometimes serial is flaky)
        print("  No test output, retrying...")
        result = flash_and_read(port)
    return result


# ── Binary Search ─────────────────────────────────────────────────────

def binary_search(macro_name, lo, hi, default, port, safety):
    """Find minimum NOP count in [lo, hi] that passes the boot test."""
    print(f"\n{'='*60}")
    print(f"  Searching {macro_name}: [{lo}, {hi}] (default={default})")
    print(f"{'='*60}")

    # 1. Verify upper bound passes
    print(f"  [{macro_name}={hi}] verifying upper bound...")
    set_nop_value(macro_name, hi)
    result = test_config(port)
    if result is not True:
        print(f"  ABORT: upper bound {hi} did not pass (result={result})")
        set_nop_value(macro_name, default)
        return default

    # 2. Binary search
    while lo < hi:
        mid = (lo + hi) // 2
        print(f"  [{macro_name}={mid}] testing (range [{lo}..{hi}])...")
        set_nop_value(macro_name, mid)
        result = test_config(port)
        if result is True:
            print(f"    PASS → try lower")
            hi = mid
        elif result is False:
            print(f"    FAIL → need more")
            lo = mid + 1
        else:
            print(f"    INDETERMINATE → treating as fail")
            lo = mid + 1

    minimum = lo
    safe = min(minimum + safety, default)  # Don't exceed original
    print(f"  ► {macro_name} minimum = {minimum}, with +{safety} safety = {safe}")

    set_nop_value(macro_name, safe)
    return safe


# ── Main ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='PDQminer NOP binary search')
    parser.add_argument('--port', default=DEFAULT_PORT)
    parser.add_argument('--safety', type=int, default=1,
                        help='Safety margin: NOPs added above minimum (default: 1)')
    parser.add_argument('--phase', type=str, default=None,
                        help='Search only this phase (e.g. NOP_57)')
    args = parser.parse_args()

    print("╔══════════════════════════════════════════════╗")
    print("║    PDQminer NOP Binary Search               ║")
    print("╚══════════════════════════════════════════════╝")
    print(f"Port: {args.port}  Safety: +{args.safety}")
    print()

    # Read current values
    originals = {}
    for name, default, lo, hi in PHASES:
        originals[name] = get_nop_value(name)
    print(f"Current values: {originals}")

    # Determine which phases to search
    phases = PHASES
    if args.phase:
        phases = [(n, d, l, h) for n, d, l, h in PHASES if n == args.phase]
        if not phases:
            print(f"ERROR: Unknown phase '{args.phase}'")
            print(f"Available: {[n for n, _, _, _ in PHASES]}")
            sys.exit(1)

    results = {}
    # Phase 1: Find individual minimums (other phases at defaults)
    individual_mins = {}
    for name, default, lo, hi in phases:
        # Reset ALL macros to their original (safe) values before each search
        for n in originals:
            set_nop_value(n, originals[n])

        minimum = binary_search(name, lo, hi, default, args.port, args.safety)
        individual_mins[name] = minimum - args.safety  # store raw minimum
        results[name] = minimum

    # Phase 2: Verify all optimized values simultaneously
    print(f"\n{'='*60}")
    print("Phase 2: Combined verification")
    print(f"{'='*60}")

    for name in results:
        set_nop_value(name, results[name])

    combined_result = test_config(args.port)
    if combined_result is True:
        print("Combined test: PASS — all optimized values work together!")
    else:
        print("Combined test: FAIL — increasing safety margins...")
        # Increase safety margin by 1 on all phases and retry
        for extra in range(1, 10):
            print(f"  Trying +{args.safety + extra} total safety...")
            for name, default, _, _ in phases:
                safe = min(individual_mins[name] + args.safety + extra, originals[name])
                results[name] = safe
                set_nop_value(name, safe)
            combined_result = test_config(args.port)
            if combined_result is True:
                print(f"  PASS with +{args.safety + extra} safety!")
                break
            print(f"  Still FAIL")
        else:
            print("  COULD NOT FIND SAFE COMBINED VALUES — reverting!")
            for name in originals:
                set_nop_value(name, originals[name])
                results[name] = originals[name]

    # Apply all optimized values (keep originals for phases not searched)
    print(f"\n{'='*60}")
    print("RESULTS:")
    print(f"{'='*60}")
    total_saved = 0
    for name, default, _, _ in PHASES:
        val = results.get(name, originals[name])
        saved = originals[name] - val
        total_saved += saved
        set_nop_value(name, val)
        marker = " ◄" if name in results else ""
        print(f"  {name}: {originals[name]:>3} → {val:>3}  (saved {saved:>2}){marker}")
    print(f"  {'─'*40}")
    print(f"  Total NOPs saved: {total_saved}")

    # Final verification with all optimized values
    print(f"\nFinal verification (all optimized)...")
    result = test_config(args.port)
    if result is True:
        print("FINAL VERIFICATION: PASS ✓")
    else:
        print(f"FINAL VERIFICATION: {result} — reverting to original values!")
        for name in originals:
            set_nop_value(name, originals[name])
        print("Reverted.")


if __name__ == '__main__':
    main()
