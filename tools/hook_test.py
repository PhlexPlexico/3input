#!/usr/bin/env python3
"""Run the IR module's C-Stick hook (patch.s, updatem_latestkeys) under an ARM emulator.

Applies two IPS files to the stock IR module and runs the hook from each on the
same inputs: the key word must be identical, and the new hook must also store the
adjusted C-Stick x/y in the ir:rst ring entry, equal to what the module's own
conversion routine (0x10F30C) returns.

    nix-shell -p 'python3.withPackages (p: [p.unicorn])' --run \
        'python3 tools/hook_test.py --stock code.bin --old old.ips --new code.ips'

code.bin is the decompressed IR module (title 0004013000003302) from your own
console. It is Nintendo's code, keep it out of the repository.
"""
import argparse
import struct
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_CPU_ARM_1176, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC, UC_ARM_REG_LR, UC_ARM_REG_R0, \
    UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP

BASE = 0x100000
HOOK = 0x10AD40
CONVERT = 0x10F30C
RING = 0x200000


def apply_ips(stock, path):
    data = bytearray(stock)
    ips = open(path, "rb").read()
    pos = 5
    while ips[pos:pos + 3] != b"EOF":
        off = int.from_bytes(ips[pos:pos + 3], "big")
        size = struct.unpack(">H", ips[pos + 3:pos + 5])[0]
        pos += 5
        if size == 0:
            count = struct.unpack(">H", ips[pos:pos + 2])[0]
            data[off:off + count] = ips[pos + 2:pos + 3] * count
            pos += 3
        else:
            data[off:off + size] = ips[pos:pos + size]
            pos += size
    return bytes(data)


def machine(image):
    mu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    mu.ctl_set_cpu_model(UC_CPU_ARM_1176)
    mu.reg_write(UC_ARM_REG_C1_C0_2, mu.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))  # enable VFP
    mu.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
    mu.mem_map(BASE, 0x20000)
    mu.mem_write(BASE, image)
    mu.mem_map(RING, 0x1000)
    mu.mem_map(0x300000, 0x1000)
    mu.mem_map(0x7F0000, 0x10000)
    # run-time calibration the conversion routine expects: centre 2048, scale 0.625 (-> about +-160)
    mu.mem_write(0x113384, b"\x00")
    mu.mem_write(0x11338C, struct.pack("<ff", 0.625, 0.625))
    mu.mem_write(0x113388, struct.pack("<hh", 2048, 2048))
    return mu


def call(mu, pc, r0, r1):
    mu.reg_write(UC_ARM_REG_R0, r0)
    mu.reg_write(UC_ARM_REG_R1, r1)
    mu.reg_write(UC_ARM_REG_R2, 0)
    mu.reg_write(UC_ARM_REG_SP, 0x7FF000)
    mu.reg_write(UC_ARM_REG_LR, 0x300800)
    mu.mem_write(0x300800, b"\x00\x00\xa0\xe1")
    mu.emu_start(pc, 0x300800, count=5000)


def run_hook(image, raw, x, y):
    mu = machine(image)
    mu.mem_write(RING + 0x10, struct.pack("<I", 5))  # newest entry = 5
    for i in range(8):
        mu.mem_write(RING + 0x18 + i * 16 + 0xC, struct.pack("<hh", 0x1111, 0x2222))
    mu.mem_write(0x113340, struct.pack("<I", RING))
    mu.mem_write(0x113344, struct.pack("<I", 0xDEADBEEF))
    mu.mem_write(0x300000, struct.pack("<I", raw))
    mu.mem_write(0x300010, struct.pack("<HHHH", x, 0, y, 0))
    call(mu, HOOK, 0x300000, 0x300010)
    keys = struct.unpack("<I", mu.mem_read(0x113344, 4))[0]
    entries = [struct.unpack("<hh", mu.mem_read(RING + 0x18 + i * 16 + 0xC, 4)) for i in range(8)]
    return keys, entries


def run_convert(image, x, y):
    mu = machine(image)
    mu.mem_write(0x300010, struct.pack("<hh", x, y))
    mu.mem_write(0x300020, struct.pack("<hh", x, y))
    call(mu, CONVERT, 0x300020, 0x300010)
    return struct.unpack("<hh", mu.mem_read(0x300020, 4))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--stock", required=True)
    ap.add_argument("--old", required=True, help="IPS to compare the key word against (e.g. rehid's)")
    ap.add_argument("--new", required=True, help="IPS under test")
    args = ap.parse_args()
    stock = open(args.stock, "rb").read()
    old, new = apply_ips(stock, args.old), apply_ips(stock, args.new)

    cases = [(0, 2048, 2048), (0, 3000, 3500), (0, 1000, 600), (0x200, 2048, 2048),
             (0x400, 3500, 2048), (0x600, 900, 3900), (0, 2060, 2040)]
    ok = True
    for raw, x, y in cases:
        keys_old, _ = run_hook(old, raw, x, y)
        keys_new, ring = run_hook(new, raw, x, y)
        expect = run_convert(new, x, y)
        same = keys_old == keys_new
        stored = ring[5] == expect and all(e == (0x1111, 0x2222) for i, e in enumerate(ring) if i != 5)
        ok &= same and stored
        print(f"raw={raw:#05x} in=({x:4d},{y:4d}) keys old={keys_old:#010x} new={keys_new:#010x} "
              f"same={same} ring[5]={ring[5]} expected={expect} ok={stored}")
    print("ALL OK" if ok else "MISMATCH")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
