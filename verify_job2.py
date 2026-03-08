#!/usr/bin/env python3
"""Verify pipeline for job 1eae295 using complete debug5 data."""

import hashlib, struct

def sha256d(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

# From debug5 session - all data from same connection
en1 = "2e1a5ba1"
en2 = "04000000"  # 4th job on this connection

# Stratum notify for job 1eae295 (from debug5)
coinbase1 = "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff170385520e5075626c69632d506f6f6c"
coinbase2 = "ffffffff024d3bb412000000001976a914b8aa2d1ea325377d3b184f15a95aa6173ade02c788ac0000000000000000266a24aa21a9eddc637c1710ee7de8538dfba8bba2c1ce70cfdc98d03d5b0ac30c0209cfa6af3000000000"
branches = [
    "55fa8652d8cfa2602cd6064fb64a207a76f882873ef941bab0ef1677b716aea7",
    "e1082daf15ed69e86f708cb8947bc01fb9a406c47f0f9e204fc710e7de205701",
    "2fdaddbae7647c26330f9349cf536e7c5ed0ab41c6c1401d2093e86baf52a2f0",
    "13f4a32bbc414f7c3a46f2fe107b2035afa640db3188e1dc126d60c0b0dc4034",
    "3cf5ae14febc0698422196e6d9a4b5d2d43c91d0c10305a14be5cd32b7e36885",
    "9dbb56c6b514ec334c794f89f48512a6fab137c9ee13efdead7f74802cde7050",
    "d97757454629489148c055844181059ea2426705a3effdaaddda55824aa214e6",
    "07374f310549eaa5c68dddba6b00f9dda5af8c85aa38c86ef5c0f8370f47dca8",
    "3736cf877f0075579c81f70dc074f54cab82624123ce4c477a092be2494e5c04",
    "3f4746d32dbd7e10b80f57a3f7a9a94daa4ccd20b29751b720c6553830fea398",
    "949d669c0da758305eaf00c3ee20dff1701242192ccd33bf286932b270ddcff9",
    "d691880147cdbd2ec53ab627db6044513815090985594d7cb691d639fab868c6",
]
version = "20000000"
nbits = "1701f303"
ntime = "69a20f9a"
prevhash = "770fd8b322f461fc7eb91584447854b4212f96070001d3360000000000000000"

# Device debug output for this job:
device_coinbase = "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff170385520e5075626c69632d506f6f6c2e1a5ba104000000ffffffff024d3bb412000000001976a914b8aa2d1ea325377d3b184f15a95aa6173ade02c788ac0000000000000000266a24aa21a9eddc637c1710ee7de8538dfba8bba2c1ce70cfdc98d03d5b0ac30c0209cfa6af3000000000"
device_cb_hash = "7139852bc343a971bc89242532f24d7a1eeef9c3a9a02652741ac2cc28f0b4d0"
device_branch0_mr = "4ec97cd85a31fd0079c4660f3f48f879c305a45f87c82b3c8ca366b40b677f70"
device_branch1_mr = "e94baeae889aa913c74d36e59868d639a853d016453bad972af16488b83f19b0"
device_merkle = "2c610898c8f28120ebe054de7112e5315f9a46fbc8db5425a891bb72fb7a2fb8"
device_header = "00000020b3d80f77fc61f4228415b97eb454784407962f2136d3010000000000000000002c610898c8f28120ebe054de7112e5315f9a46fbc8db5425a891bb72fb7a2fb89a0fa26903f3011700000000"

# Python construction
print("Step 1: Coinbase")
py_coinbase = bytes.fromhex(coinbase1 + en1 + en2 + coinbase2)
print(f"  Match device: {py_coinbase.hex() == device_coinbase}")

print("\nStep 2: Coinbase hash")
py_cb_hash = sha256d(py_coinbase)
print(f"  Python:  {py_cb_hash.hex()}")
print(f"  Device:  {device_cb_hash}")
print(f"  Match:   {py_cb_hash.hex() == device_cb_hash}")

print("\nStep 3: Merkle root")
mr = py_cb_hash
for i, b in enumerate(branches):
    mr = sha256d(mr + bytes.fromhex(b))
    if i == 0:
        print(f"  Branch[0] MR python:  {mr.hex()}")
        print(f"  Branch[0] MR device:  {device_branch0_mr}")
        print(f"  Match: {mr.hex() == device_branch0_mr}")
    elif i == 1:
        print(f"  Branch[1] MR python:  {mr.hex()}")
        print(f"  Branch[1] MR device:  {device_branch1_mr}")
        print(f"  Match: {mr.hex() == device_branch1_mr}")

print(f"\n  Final MR python: {mr.hex()}")
print(f"  Final MR device: {device_merkle}")
print(f"  Match: {mr.hex() == device_merkle}")

print("\nStep 4: Header")
v = struct.pack('<I', int(version, 16))
ph = b''
for i in range(0, 32, 4):
    ph += bytes.fromhex(prevhash)[i:i+4][::-1]
nt = struct.pack('<I', int(ntime, 16))
nb = struct.pack('<I', int(nbits, 16))
py_header = v + ph + mr + nt + nb + b'\x00\x00\x00\x00'
print(f"  Python: {py_header.hex()}")
print(f"  Device: {device_header}")
print(f"  Match:  {py_header.hex() == device_header}")

if py_header.hex() == device_header:
    print("\n*** FULL PIPELINE VERIFIED! Python matches device exactly. ***")
else:
    for i in range(80):
        if py_header[i] != bytes.fromhex(device_header)[i]:
            print(f"  First diff at byte {i}")
            break
