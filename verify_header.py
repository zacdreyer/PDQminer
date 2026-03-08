#!/usr/bin/env python3
"""Verify block header construction from captured debug data."""

import hashlib
import struct

def sha256d(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def display_hash(h):
    """Display hash in conventional big-endian hex."""
    return h[::-1].hex()

# Header from debug output (first job after prevhash fix)
header_hex = "000000200a5cf095340ba3df334d757e4912580e467727e6b76700000000000000000000cc86de19ca52c3d2cc7862213ce461327c757bacac1f21e48248f227f381b395e509a26903f3011700000000"
header = bytes.fromhex(header_hex)
print(f"Header length: {len(header)} bytes")

# Parse fields
version = int.from_bytes(header[0:4], 'little')
prevhash = header[4:36]
merkle = header[36:68]
ntime = int.from_bytes(header[68:72], 'little')
nbits = int.from_bytes(header[72:76], 'little')
nonce = int.from_bytes(header[76:80], 'little')

print(f"Version: 0x{version:08x}")
print(f"PrevHash display: {display_hash(prevhash)}")
print(f"MerkleRoot display: {display_hash(merkle)}")
print(f"NTime: 0x{ntime:08x} = {ntime}")
print(f"NBits: 0x{nbits:08x}")
print(f"Nonce: {nonce}")

# Compute SHA256d with nonce=0
h = sha256d(header)
print(f"\nSHA256d (nonce=0): {display_hash(h)}")

# Now verify prevhash decoding from stratum
stratum_prev = "95f05c0adfa30b347e754d330e581249e6277746000067b70000000000000000"
stratum_bytes = bytes.fromhex(stratum_prev)

# Method 1: per-group bswap (current code)
decoded_bswap = b''
for i in range(0, 32, 4):
    decoded_bswap += stratum_bytes[i:i+4][::-1]
print(f"\nStratum prevhash decode (per-group bswap):")
print(f"  Header bytes: {decoded_bswap.hex()}")
print(f"  Matches header[4:36]: {decoded_bswap == prevhash}")
print(f"  Display: {display_hash(decoded_bswap)}")

# Method 2: full reverse (OLD broken code)
decoded_fullrev = stratum_bytes[::-1]
print(f"\nStratum prevhash decode (full reverse):")
print(f"  Header bytes: {decoded_fullrev.hex()}")
print(f"  Matches header[4:36]: {decoded_fullrev == prevhash}")
print(f"  Display: {display_hash(decoded_fullrev)}")

# Verify if prevhash display looks like a real recent block hash
# Should have ~19-20 leading hex zeros for current Bitcoin difficulty
prevhash_display = display_hash(decoded_bswap)
leading_zeros = len(prevhash_display) - len(prevhash_display.lstrip('0'))
print(f"\n  Leading zeros in prevhash: {leading_zeros} hex chars")

print("\n" + "="*60)
print("STRATUM DATA RECONSTRUCTION")
print("="*60)

# Captured stratum notify data (from /tmp/pdq_full.txt)
stratum_version = "20000000"
stratum_prevhash = "95f05c0adfa30b347e754d330e581249e6277746000067b70000000000000000"
stratum_nbits = "1701f303"
stratum_ntime = "69a209e5"

# Decode version: stratum sends as hex BE
version_decoded = struct.unpack('>I', bytes.fromhex(stratum_version))[0]
# In block header, version is LE
version_le = struct.pack('<I', version_decoded)
print(f"Version: stratum={stratum_version} -> value={version_decoded:#010x} -> LE bytes={version_le.hex()}")
print(f"  matches header[0:4]: {version_le == header[0:4]}")

# Decode ntime: stratum hex -> uint32 value -> LE in header
ntime_val = int(stratum_ntime, 16)
ntime_le = struct.pack('<I', ntime_val)
print(f"NTime: stratum={stratum_ntime} -> value={ntime_val:#010x} -> LE bytes={ntime_le.hex()}")
print(f"  matches header[68:72]: {ntime_le == header[68:72]}")

# Decode nbits: stratum hex -> uint32 value -> LE in header
nbits_val = int(stratum_nbits, 16)
nbits_le = struct.pack('<I', nbits_val)
print(f"NBits: stratum={stratum_nbits} -> value={nbits_val:#010x} -> LE bytes={nbits_le.hex()}")
print(f"  matches header[72:76]: {nbits_le == header[72:76]}")

print("\n" + "="*60)
print("MERKLE ROOT VERIFICATION (needs extranonce1)")
print("="*60)

# Coinbase data
coinbase1 = "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff170382520e5075626c69632d506f6f6c"
coinbase2 = "ffffffff024c52db12000000001976a914b8aa2d1ea325377d3b184f15a95aa6173ade02c788ac0000000000000000266a24aa21a9edced15307bda909434b89974e382c3083c12cd5cba0deb921cca0e98aca1e28a200000000"

# Merkle branches (captured from stratum notify)
merkle_branches = [
    "c9f2b33ee19a6b2b23b62b6dab2aea6b4f5ac5dba0f000ac0fdc44b76b5f3fa1",
    "b1e2e0e7b1a15ac1314dfaa7ec5e9f4f6bef85e4e94c57b5f1e8137e209e436c",
    "dc8ceee1e8ad67ad2a8dc1aa4e8f319e1f1b2d1a2fdcc98c7ed8c8e15fadf9bd",
    "d2a5f1fccc8f4c10ea23d77a42ae65be1a16ad33ca9ef42d47aae81bf194cff9",
    "bcda88da29e5c7afef0e6e09a24a5b95ffbe4afd7c8d3424cba7571dcb9dce82",
    "cd5caa33c4ac5a6cba2cf0c8e5dd76df2ccaa0ab93dced37e2e436f79ec6f1a8",
    "0f8b41e29a456a50b6e94a10ac65a20f87c966b3d9e8d930febbe2b4b9e5c3c3",
    "b1e43b2a962e9e35e6d67fd5db01ed2e4e1a0ff39e08e6de7e6d7cddc7fba59c",
    "eb2c4f3ec39fbd3b4d3db40aeb16f6f9ae45c9ea78fc48c91ae9bd22eb27cd18",
    "09e4e74f69ff9406c1ef9a7e76ed2aec2e7ff6ee7bac4e74de2d4d1f5c7a00f9",
    "f6caa9d26e9c2c70a90a2c3e7e83f0ab73c3b5c7f7e3a0c3b3e367c2a4cda577",
    "fc8c4adcdbd04d485fda6e8aa8f2d6e6adab4da12c8a5ea7ee6afe88f1fad94a"
]

# Since we don't know extranonce1, let's try common sizes (3 or 4 bytes)
# and see which one produces the merkle root in the header
print(f"\nTarget merkle root (from header): {merkle.hex()}")
print(f"  Display: {display_hash(merkle)}")

# Test: if we knew the full coinbase, compute merkle root
# coinbase = coinbase1_bytes + extranonce1_bytes + extranonce2_bytes + coinbase2_bytes
# For extranonce2 = "02000000" (4 bytes)
extranonce2 = bytes.fromhex("02000000")

# Try to brute-force extranonce1 by trying a few common patterns
# Actually, we can compute what merkle_step0 should be by working backwards
# MR = hash(hash(...hash(hash(coinbase_hash + branch[0]) + branch[1])... + branch[11])
# So if we know MR and branches, we can work backwards to find coinbase_hash
# Then from coinbase_hash we can find the extranonce1

# Actually, let's just focus on the header byte order issue
# The key question: does the merkle root in the header need per-group bswap?
# In standard Bitcoin, the merkle root goes into the header as-is (raw SHA256d output bytes)
# Let's see if our code does any byte swapping on it

print("\nNOTE: Cannot verify merkle root without extranonce1.")
print("Focus: verify that version, prevhash, ntime, nbits byte order is correct.")

# Summary
print("\n" + "="*60)
print("VERIFICATION SUMMARY")
print("="*60)
print(f"Version field:  {'OK' if version_le == header[0:4] else 'MISMATCH'}")
print(f"PrevHash field: {'OK' if decoded_bswap == prevhash else 'MISMATCH'}")
print(f"NTime field:    {'OK' if ntime_le == header[68:72] else 'MISMATCH'}")
print(f"NBits field:    {'OK' if nbits_le == header[72:76] else 'MISMATCH'}")
print(f"Nonce field:    {nonce} (expected 0 for template)")

# Let's also check: what does the pool expect for submit?
# mining.submit params: worker, job_id, extranonce2, ntime, nonce
# ntime: big-endian hex string of uint32
# nonce: big-endian hex string of uint32
# extranonce2: hex string of LE bytes
print(f"\n{'='*60}")
print("SUBMIT FORMAT CHECK")
print(f"{'='*60}")
# From log: submit nonce="23567a4d", ntime="69a20556"
submit_nonce_hex = "23567a4d"
submit_ntime_hex = "69a20556"
print(f"Submitted nonce hex: {submit_nonce_hex}")
print(f"  As uint32 (parsing as BE hex): {int(submit_nonce_hex, 16):#010x} = {int(submit_nonce_hex, 16)}")
print(f"  In header (LE bytes): {struct.pack('<I', int(submit_nonce_hex, 16)).hex()}")
print(f"  In header (BE bytes): {struct.pack('>I', int(submit_nonce_hex, 16)).hex()}")

# When we submit nonce as "%08x" of uint32 Nonce, does pool expect:
# The nonce value itself, or some byte-swapped version?
# Standard: submit nonce as hex of the LITTLE-ENDIAN byte representation
# Wait no - stratum protocol sends nonce as hex(LE_bytes), same as version
# Actually, stratum sends nonce as the 4-byte hex string of header bytes 76-79
print(f"\n  Pool interprets nonce hex as the 4 LE bytes in header[76:80]")
print(f"  So header[76:80] = bytes.fromhex('{submit_nonce_hex}') = {bytes.fromhex(submit_nonce_hex).hex()}")

# Check: in our C code, we do sprintf(NonceStr, "%08x", Nonce)
# where Nonce is the uint32 value. On LE machine, Nonce bytes in memory are LE.
# The block header has nonce as LE uint32.
# When we mine, we put Bswap32(Nonce) into Block1Buf[3] which is the SHA input.
# The SHA engine operates on BE words, so Bswap32(Nonce) in BE word = Nonce in LE = correct.
# When we submit, sprintf("%08x", Nonce) gives the hex of the VALUE, which is BE representation.
# But stratum expects hex of the LE BYTES.
# Example: if Nonce value = 0x4d7a5623
#   sprintf("%08x", Nonce) = "4d7a5623" (BE hex of value)
#   But pool wants LE bytes: 23 56 7a 4d -> "23567a4d"
# So our submit is BACKWARDS!
print(f"\n{'='*60}")
print("CRITICAL: NONCE SUBMIT BYTE ORDER")
print(f"{'='*60}")
print("If our C code does: sprintf(\"%08x\", Nonce)")
print("  Nonce value 0x4d7a5623 -> submit '4d7a5623' (BE hex)")
print("  But pool expects LE bytes: '23567a4d'")
print("  These are DIFFERENT!")
print("")
print("Actually wait - let's check what cgminer does...")
print("cgminer submits nonce as: hex of header[76:80] bytes = LE bytes of uint32")
print("Our code does sprintf('%08x', Nonce) = hex of value = BE representation")
print("")
print("IF submitted nonce '23567a4d' came from sprintf('%08x', 0x23567a4d),")
print("  then the VALUE is 0x23567a4d")
print("  LE bytes in header: 4d 7a 56 23")
print("  Pool reconstructs: header[76:80] = bytes.fromhex('23567a4d') = 23 56 7a 4d")
print("  Pool's nonce VALUE: int.from_bytes(b'\\x23\\x56\\x7a\\x4d', 'little') =", int.from_bytes(b'\x23\x56\x7a\x4d', 'little'))
print("  Our nonce VALUE: 0x23567a4d =", 0x23567a4d)
print("  MISMATCH! Pool thinks nonce is", hex(int.from_bytes(b'\x23\x56\x7a\x4d', 'little')), "but we computed", hex(0x23567a4d))
print("")

# Actually, we need to check what the standard format actually is.
# Stratum v1: nonce is submitted as the hex string of the 4 nonce bytes AS THEY APPEAR in the block header
# In the block header, nonce is at offset 76, stored as uint32 LE
# So if nonce value = 0x23567a4d, header bytes = 4d 7a 56 23, hex = "4d7a5623"
# If our code does sprintf("%08x", 0x23567a4d) = "23567a4d"
# These are SWAPPED! The pool would reconstruct the wrong nonce value.

# However, some pools/implementations disagree on this. Let's check what the accepted practice is.
# In cgminer: the nonce is sent as the 4 bytes at work->data[76..79] in hex
# Since data[76..79] contains the LE representation of the nonce uint32:
#   data[76]=4d, data[77]=7a, data[78]=56, data[79]=23 (for value 0x23567a4d)
# cgminer sends: "4d7a5623"

# Our code sends: "23567a4d" (from sprintf("%08x", Nonce))
# This is WRONG - it's byte-swapped!

print("CONCLUSION: Our nonce submission is byte-swapped!")
print("We submit sprintf('%08x', Nonce) which gives BE hex of the value.")
print("Pool expects LE bytes hex (as they appear in the block header).")
print("Fix: submit as sprintf('%02x%02x%02x%02x', nonce&0xff, (nonce>>8)&0xff, ...)")
print("Or: submit sprintf('%08x', bswap32(Nonce))")
