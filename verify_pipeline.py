#!/usr/bin/env python3
"""Verify full block header construction pipeline against device output."""

import hashlib
import struct

def sha256d(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

# ============================================================
# DATA FROM DEVICE DEBUG OUTPUT
# ============================================================
device_extranonce1 = "bffa1d50"
device_extranonce2 = "02000000"
device_coinbase_hex = "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff170384520e5075626c69632d506f6f6cbffa1d5002000000ffffffff020166b912000000001976a914b8aa2d1ea325377d3b184f15a95aa6173ade02c788ac0000000000000000266a24aa21a9ed294da4faa5e1bab0b2b687a2c3e1c99898c58bde73266531733f0fe7590417e500000000"
device_merkle_root = "53e113865a37d190010881f751c3f6db2e8c00336f15dc7c804b763d457421f2"
device_header_hex = "000000209a7fe8085978e21682803c7152a7a864853e21fc7b7d01000000000000000000" + \
                    "53e113865a37d190010881f751c3f6db2e8c00336f15dc7c804b763d457421f2" + \
                    "cd0ca26903f3011700000000"

# ============================================================
# DATA FROM STRATUM NOTIFY
# ============================================================
stratum_prevhash = "08e87f9a16e27859713c808264a8a752fc213e8500017d7b0000000000000000"
stratum_coinbase1 = "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff170384520e5075626c69632d506f6f6c"
stratum_coinbase2 = "ffffffff020166b912000000001976a914b8aa2d1ea325377d3b184f15a95aa6173ade02c788ac0000000000000000266a24aa21a9ed294da4faa5e1bab0b2b687a2c3e1c99898c58bde73266531733f0fe7590417e500000000"
stratum_merkle_branches = [
    "e845fe642fca3ff8edb965a90b95be26ed7f86240e2b8991f0cd81f0440c67c8",
    "275fde04bd44d5bba31ca29fc93421c8eeecfe515b91bc0e92445de4e7e6a4ca",
    "b312f3ad810c972a94f4ba162af4e9981c3e0e67421eff8c04dc05100f112f2b",
    "a1b883d59e6780a227850bfca3bdf551bcc989d9b4d5f3038ebab798ce029919",
    "b6b9c306288651609d29e5140fbdf69909a2e1d48d63b7b2f1fe26ba97e42db1",
    "a2c44f1ea55f8ed59c0021dde766194f799975285456837aee40716a23fbb88c",
    "a41b905b345fa3ac55c6401b8349b3cbb9badb56df3c2dd617ec3e6b2e88b40b",
    "54219eb2a03674cc671b282319952fd0c529cd1ed043c17f9a46d839ac5cc149",
    "f4adf56ad61ae09a4534689fb65648efb5d1b521ec64c971306dd0847dfeee18",
    "3e23032a83446d96a910242c167bce3d881593c6ef1d0a70281a31b652815ef7",
    "529e5cb849c354451b02c093a34f6c309d5fa091a6bc31c3cf664797f0c797ac",
    "fd551fa11b3acb3236b2134f732fa90282bd7b5b6ffa5e6232df27efadb54fa9",
]
stratum_version = "20000000"
stratum_nbits = "1701f303"
stratum_ntime = "69a20ccd"

print("=" * 60)
print("STEP 1: COINBASE CONSTRUCTION")
print("=" * 60)

# Construct coinbase from stratum data
coinbase1_bytes = bytes.fromhex(stratum_coinbase1)
extranonce1_bytes = bytes.fromhex(device_extranonce1)
extranonce2_bytes = bytes.fromhex(device_extranonce2)
coinbase2_bytes = bytes.fromhex(stratum_coinbase2)

python_coinbase = coinbase1_bytes + extranonce1_bytes + extranonce2_bytes + coinbase2_bytes
device_coinbase = bytes.fromhex(device_coinbase_hex)

print(f"Python coinbase len: {len(python_coinbase)}")
print(f"Device coinbase len: {len(device_coinbase)}")
print(f"Coinbase match: {python_coinbase == device_coinbase}")
if python_coinbase != device_coinbase:
    for i in range(min(len(python_coinbase), len(device_coinbase))):
        if python_coinbase[i] != device_coinbase[i]:
            print(f"  FIRST DIFF at byte {i}: python={python_coinbase[i]:02x} device={device_coinbase[i]:02x}")
            break

print(f"\n{'=' * 60}")
print("STEP 2: COINBASE HASH (SHA256d)")
print("=" * 60)

coinbase_hash = sha256d(python_coinbase)
print(f"Coinbase SHA256d: {coinbase_hash.hex()}")

print(f"\n{'=' * 60}")
print("STEP 3: MERKLE ROOT COMPUTATION")
print("=" * 60)

merkle = coinbase_hash
for i, branch_hex in enumerate(stratum_merkle_branches):
    branch = bytes.fromhex(branch_hex)
    concat = merkle + branch
    merkle = sha256d(concat)
    if i < 3 or i >= len(stratum_merkle_branches) - 1:
        print(f"  After branch {i}: {merkle.hex()}")

python_merkle_root = merkle
device_merkle = bytes.fromhex(device_merkle_root)

print(f"\nPython merkle root: {python_merkle_root.hex()}")
print(f"Device merkle root: {device_merkle.hex()}")
print(f"Merkle root match:  {python_merkle_root == device_merkle}")

print(f"\n{'=' * 60}")
print("STEP 4: PREVHASH DECODING")
print("=" * 60)

stratum_bytes = bytes.fromhex(stratum_prevhash)
# Per-group bswap (what our code does)
decoded_prevhash = b''
for i in range(0, 32, 4):
    decoded_prevhash += stratum_bytes[i:i+4][::-1]

print(f"Stratum prevhash:  {stratum_prevhash}")
print(f"Decoded (bswap32): {decoded_prevhash.hex()}")

# What's in the device header bytes 4-36
device_header = bytes.fromhex(device_header_hex)
device_prevhash = device_header[4:36]
print(f"Device header[4:36]: {device_prevhash.hex()}")
print(f"Prevhash match:      {decoded_prevhash == device_prevhash}")

print(f"\n{'=' * 60}")
print("STEP 5: FULL HEADER CONSTRUCTION")
print("=" * 60)

# Version: hex string → uint32 → LE in header
version_val = int(stratum_version, 16)
version_le = struct.pack('<I', version_val)

# NTime: hex string → uint32 → LE in header
ntime_val = int(stratum_ntime, 16)
ntime_le = struct.pack('<I', ntime_val)

# NBits: hex string → uint32 → LE in header
nbits_val = int(stratum_nbits, 16)
nbits_le = struct.pack('<I', nbits_val)

python_header = version_le + decoded_prevhash + python_merkle_root + ntime_le + nbits_le + b'\x00\x00\x00\x00'

print(f"Python header: {python_header.hex()}")
print(f"Device header: {device_header_hex}")
print(f"Header match:  {python_header.hex() == device_header_hex}")

if python_header.hex() != device_header_hex:
    dev_h = bytes.fromhex(device_header_hex)
    for i in range(80):
        if python_header[i] != dev_h[i]:
            print(f"  FIRST DIFF at byte {i}: python={python_header[i]:02x} device={dev_h[i]:02x}")
            break

print(f"\n{'=' * 60}")
print("STEP 6: SHA256d OF HEADER (with nonce=0)")
print("=" * 60)

hash_result = sha256d(python_header)
print(f"SHA256d: {hash_result[::-1].hex()}")

print(f"\n{'=' * 60}")
print("STEP 7: DIFFICULTY TARGET CHECK")
print("=" * 60)

# Target for diff=1.0: 00000000ffff0000...
# In LE uint256 pn[] format: pn[7]=0, pn[6]=0xFFFF0000, pn[5..0]=0
target_hex = "00000000000000000000000000000000000000000000000000000000ffff000000000000"
# Actually, let's compute it properly
# Diff 1.0 target = 0x00000000FFFF0000000000000000000000000000000000000000000000000000
# As LE bytes (for header comparison): the above is already BE display

# For a share to be valid at diff=1.0, the hash display must have leading zeros >= 8 hex chars
# (i.e., first 4 bytes of display hash must be 0)
hash_display = hash_result[::-1].hex()
print(f"Hash display: {hash_display}")
leading_zeros = len(hash_display) - len(hash_display.lstrip('0'))
print(f"Leading zeros: {leading_zeros}")
print(f"Need >= 8 for diff=1.0: {'PASS' if leading_zeros >= 8 else 'FAIL'}")

print(f"\n{'=' * 60}")
print("SUMMARY")
print("=" * 60)
print(f"Coinbase:    {'OK' if python_coinbase == device_coinbase else 'MISMATCH'}")
print(f"Merkle root: {'OK' if python_merkle_root == device_merkle else 'MISMATCH'}")
print(f"Prevhash:    {'OK' if decoded_prevhash == device_prevhash else 'MISMATCH'}")
print(f"Full header: {'OK' if python_header.hex() == device_header_hex else 'MISMATCH'}")

if python_header.hex() == device_header_hex:
    print("\n*** HEADER IS CORRECT! ***")
    print("The issue must be in the SHA256d mining engine or share submission.")
else:
    print("\n*** HEADER MISMATCH - FIX NEEDED ***")
