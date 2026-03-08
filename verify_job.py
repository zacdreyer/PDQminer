#!/usr/bin/env python3
"""Verify pipeline for the specific rejected job 1eaa720."""

import hashlib, struct

def sha256d(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

# From debug5 output - EN1 is same for all jobs on the connection
en1 = "2e1a5ba1"
en2 = "02000000"  # This share

# From debug4 - the stratum notify for job 1eaa720
coinbase1 = "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff170385520e5075626c69632d506f6f6c"
coinbase2 = "ffffffff029d37ad12000000001976a914b8aa2d1ea325377d3b184f15a95aa6173ade02c788ac0000000000000000266a24aa21a9ed6d1579af07100699d569f5cb3c421dc0330015a1e4ddf5042b25f0749ca021e100000000"
branches = [
    "55fa8652d8cfa2602cd6064fb64a207a76f882873ef941bab0ef1677b716aea7",
    "38d5a4580cc5606a43947121779ccb399eca779701594874bc6660c48fa2674c",
    "d0a5c8ba7f39f23fdcca692b7d663648bf7cb28319c552d86c972abf2b59c455",
    "dcfa4230584c81a7b343e34586d547a3e8656fc88a208b11fec898bb1548670f",
    "d69e3f81dca1fa790ec02492a54e6cb7efc8cad946edc06ac9efc632a03262c9",
    "a106583fcf6476d85c633ffec87ca451c890f7dec6316c5ad70bb08db9fbf1d4",
    "4151d18690fb05995a14d8dc17c2b78bcd03096acb694a2338c992e0c052efe5",
    "c708d65d159bbcd09b435d1363704f9bebc92f2eae5b19af1975aa888e6cb8ba",
    "f0cd715d09a9145b777b828e5763c260ecebf0d8bc9b16110cb25c08ea095a04",
    "ade63f6d170b223a07f1e5a5f831546c6a98a612b413184e6467d568d8bb8708",
    "7386bcc5fcd7ce7207a80f53af8a2fb66f616fcefd7a7e0ffd17dd4ea32a6fb3",
]
version = "20000000"
nbits = "1701f303"
ntime = "69a20ee6"
prevhash = "770fd8b322f461fc7eb91584447854b4212f96070001d3360000000000000000"

# Submitted share data:
submit_nonce = "24955d6b"
submit_ntime = "69a20ee6"
submit_en2 = "02000000"

print("Step 1: Coinbase")
coinbase = bytes.fromhex(coinbase1 + en1 + en2 + coinbase2)
print(f"  len={len(coinbase)}")

print("\nStep 2: Coinbase hash")
cb_hash = sha256d(coinbase)
print(f"  {cb_hash.hex()}")

print("\nStep 3: Merkle root")
mr = cb_hash
for i, b in enumerate(branches):
    mr = sha256d(mr + bytes.fromhex(b))
    if i < 2: print(f"  branch[{i}]: {b[:16]}... -> {mr.hex()}")
print(f"  Final MR: {mr.hex()}")

print("\nStep 4: Build header")
v = struct.pack('<I', int(version, 16))
ph = b''
for i in range(0, 32, 4):
    ph += bytes.fromhex(prevhash)[i:i+4][::-1]
nt = struct.pack('<I', int(ntime, 16))
nb = struct.pack('<I', int(nbits, 16))
nonce_val = int(submit_nonce, 16)
nc = struct.pack('<I', nonce_val)
header = v + ph + mr + nt + nb + nc
print(f"  Header: {header.hex()}")

print("\nStep 5: SHA256d of header")
h = sha256d(header)
display = h[::-1].hex()
print(f"  Hash: {display}")
leading = len(display) - len(display.lstrip('0'))
print(f"  Leading zeros: {leading}")

# Diff 1.0 target: 00000000ffff0000...
# Hash must be < target to be valid
# That means first 4 bytes of display hash must be 0
target = 0x00000000FFFF0000 << 192
hash_int = int.from_bytes(h, 'little')  # LE uint256
print(f"\n  Hash value < target: {hash_int < target}")
if hash_int < target:
    print("  *** SHARE WOULD BE VALID at diff=1 ***")
else:
    print(f"  Share does NOT meet diff=1")
    # What difficulty does it actually achieve?
    if hash_int > 0:
        actual_diff = (0xFFFF * (2**208)) / hash_int
        print(f"  Actual difficulty: {actual_diff:.6f}")

# Also compute header with nonce=0 to compare with device debug output
header0 = v + ph + mr + nt + nb + b'\x00\x00\x00\x00'
print(f"\n  Header (nonce=0): {header0.hex()}")
