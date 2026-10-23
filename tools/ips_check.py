#!/usr/bin/env python3
"""Decode an IPS patch and check it against the armips source it came from.

    tools/ips_check.py code.ips                 # decode hunks (+ ARM disassembly)
    tools/ips_check.py code.ips patch.s         # also rebuild patch.s and compare
    tools/ips_check.py --build out.ips --stock code.bin patch.s
                                                # generate an IPS from patch.s
                                                # (only the bytes that differ from the stock module)

patch.s is written for armips, which is not required here: the file is
translated to GNU as syntax (every .org becomes its own section, linked at
that address) and assembled with the devkitARM toolchain (arm-none-eabi-*
must be on PATH). IPS offsets are relative to the start of code.bin, which
Luma maps at 0x100000.
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

TOOLCHAIN = "arm-none-eabi-"


def parse_ips(data):
    """Returns [(offset, bytes)]; RLE records are expanded."""
    if data[:5] != b"PATCH":
        raise SystemExit("not an IPS file (missing PATCH header)")
    pos, hunks = 5, []
    while True:
        if data[pos:pos + 3] == b"EOF":
            break
        offset = int.from_bytes(data[pos:pos + 3], "big")
        size = struct.unpack(">H", data[pos + 3:pos + 5])[0]
        pos += 5
        if size == 0:  # RLE
            count = struct.unpack(">H", data[pos:pos + 2])[0]
            payload = data[pos + 2:pos + 3] * count
            pos += 3
        else:
            payload = data[pos:pos + size]
            pos += size
        hunks.append((offset, payload))
    return hunks


def tool(name):
    return shutil.which(TOOLCHAIN + name)


def disassemble(tmp, base, payload):
    objdump = tool("objdump")
    if not objdump:
        return None
    raw = os.path.join(tmp, "dis.bin")
    with open(raw, "wb") as f:
        f.write(payload)
    out = subprocess.run(
        [objdump, "-D", "-b", "binary", "-marm", f"--adjust-vma={base:#x}", raw],
        capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if re.match(r"\s+[0-9a-f]+:", l)]


def translate(src):
    """Split armips patch.s into (address, gnu-as text) chunks."""
    chunks, cur = [], None
    for line in src.splitlines():
        stripped = line.split("//")[0].strip()
        m = re.match(r"\.org\s+(0x[0-9a-fA-F]+|\d+)", stripped)
        if m:
            cur = [int(m.group(1), 0), []]
            chunks.append(cur)
            continue
        if cur is None or not stripped:
            continue
        if re.match(r"\.(3ds|open|close)\b", stripped):
            continue
        # armips encodes these as stmfd/ldmfd/mov, GNU as picks str/ldr/nop-hint
        stripped = re.sub(r"^push\s*\{(.*)\}$", r"stmfd sp!, {\1}", stripped)
        stripped = re.sub(r"^pop\s*\{(.*)\}$", r"ldmfd sp!, {\1}", stripped)
        stripped = re.sub(r"^nop$", "mov r0, r0", stripped)
        stripped = re.sub(r"^\.dcb\b", ".byte", stripped)
        stripped = re.sub(r"^\.pool\b", ".ltorg", stripped)
        cur[1].append(stripped)
    return chunks


def assemble(src_text, tmp):
    cc_as, ld, objcopy = tool("as"), tool("ld"), tool("objcopy")
    if not (cc_as and ld and objcopy):
        raise SystemExit("arm-none-eabi-{as,ld,objcopy} not found, set up devkitARM first")
    chunks = translate(src_text)
    asm = ["    .syntax unified", "    .arm"]
    script = ["SECTIONS {"]
    for i, (addr, lines) in enumerate(chunks):
        asm.append(f'    .section .c{i},"ax"')
        asm += ["    " + l if not l.endswith(":") else l for l in lines]
        script.append(f"  .c{i} {addr:#x} : {{ *(.c{i}) }}")
    script.append("}")
    s, o, lds, elf = (os.path.join(tmp, n) for n in ("p.s", "p.o", "p.ld", "p.elf"))
    open(s, "w").write("\n".join(asm) + "\n")
    open(lds, "w").write("\n".join(script) + "\n")
    subprocess.run([cc_as, "-march=armv6k", s, "-o", o], check=True)
    subprocess.run([ld, "-T", lds, "-e", "0", o, "-o", elf], check=True)
    result = []
    for i, (addr, _) in enumerate(chunks):
        out = os.path.join(tmp, f"c{i}.bin")
        subprocess.run([objcopy, "-O", "binary", "-j", f".c{i}", elf, out], check=True)
        result.append((addr, open(out, "rb").read()))
    return result


def build_ips(out_path, stock_path, patch_s, base):
    """Assemble patch.s and write an IPS holding only the bytes that differ from stock."""
    stock = open(stock_path, "rb").read()
    tmp = tempfile.mkdtemp()
    hunks = []
    for addr, data in assemble(open(patch_s).read(), tmp):
        off = addr - base
        old = stock[off:off + len(data)]
        if len(old) != len(data):
            raise SystemExit(f"{addr:#x} is outside the stock image")
        diff = [i for i in range(len(data)) if data[i] != old[i]]
        if not diff:
            continue
        first, last = diff[0], diff[-1]
        hunks.append((off + first, data[first:last + 1]))
    hunks.sort()
    blob = b"PATCH"
    for off, payload in hunks:
        if off == 0x454F46:
            raise SystemExit("hunk offset collides with the IPS EOF marker")
        blob += off.to_bytes(3, "big") + struct.pack(">H", len(payload)) + payload
    blob += b"EOF"
    open(out_path, "wb").write(blob)
    print(f"wrote {out_path}: {len(hunks)} hunk(s), {len(blob)} bytes")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("ips", nargs="?")
    ap.add_argument("patch_s", nargs="?")
    ap.add_argument("--build", metavar="OUT.ips", help="assemble patch.s and write an IPS here (needs --stock)")
    ap.add_argument("--stock", metavar="code.bin", help="the unmodified, decompressed IR module code")
    ap.add_argument("--base", type=lambda x: int(x, 0), default=0x100000,
                    help="virtual address of code.bin offset 0 (default 0x100000)")
    args = ap.parse_args()

    if args.build:
        if not (args.stock and (args.patch_s or args.ips)):
            raise SystemExit("--build needs --stock code.bin and the patch.s path")
        return build_ips(args.build, args.stock, args.patch_s or args.ips, args.base)
    if not args.ips:
        ap.error("an IPS file is required")
    hunks = parse_ips(open(args.ips, "rb").read())
    tmp = tempfile.mkdtemp()
    print(f"{args.ips}: {len(hunks)} hunk(s), {sum(len(p) for _, p in hunks)} bytes\n")
    ips_bytes = {}
    for off, payload in hunks:
        va = args.base + off
        print(f"hunk offset {off:#08x}  va {va:#08x}  size {len(payload):#x}")
        for i, b in enumerate(payload):
            ips_bytes[va + i] = b
        dis = disassemble(tmp, va, payload) if len(payload) >= 4 and len(payload) % 4 == 0 else None
        if dis:
            print("\n".join("    " + l.strip() for l in dis))
        else:
            print("    " + payload.hex(" "))
    if not args.patch_s:
        return 0

    built = assemble(open(args.patch_s).read(), tmp)
    print(f"\n== comparing against {args.patch_s} ==")
    explained, bad = set(), 0
    for addr, data in built:
        explained.update(range(addr, addr + len(data)))
        covered = [i for i in range(len(data)) if addr + i in ips_bytes]
        if not covered:
            print(f"  {addr:#08x} +{len(data):#x}: NOT in IPS")
            bad += 1
            continue
        diff = [addr + i for i in covered if ips_bytes[addr + i] != data[i]]
        if diff:
            got = bytes(ips_bytes.get(addr + i, 0) for i in range(len(data)))
            print(f"  {addr:#08x} +{len(data):#x}: DIFFERS at {', '.join(f'{d:#x}' for d in diff[:8])}"
                  f"\n      patch.s {data.hex(' ')}\n      ips     {got.hex(' ')}")
            bad += 1
        elif len(covered) < len(data):
            # An IPS only stores bytes that differ from the stock module, so
            # bytes the new instruction happens to share with stock are absent.
            print(f"  {addr:#08x} +{len(data):#x}: match ({len(covered)} changed byte(s) in IPS, "
                  f"{len(data) - len(covered)} identical to stock, not checkable without code.bin)")
        else:
            print(f"  {addr:#08x} +{len(data):#x}: match")

    # Bytes the IPS carries that patch.s does not produce. A few bytes squeezed
    # between two patched regions are just unchanged code.bin content that the
    # IPS generator merged into one hunk; anything else is unexplained.
    lo, hi = min(explained), max(explained)
    fill = sorted(a for a in set(ips_bytes) - explained if lo <= a <= hi
                  and any(a - k in explained for k in range(1, 9))
                  and any(a + k in explained for k in range(1, 9)))
    other = sorted(set(ips_bytes) - explained - set(fill))
    if fill:
        print(f"  {len(fill)} byte(s) between patched regions, assumed unchanged code.bin data: "
              + ", ".join(f"{a:#x}={ips_bytes[a]:02x}" for a in fill))
    if other:
        print(f"  {len(other)} IPS byte(s) with no counterpart in patch.s "
              f"({other[0]:#x}..{other[-1]:#x})")
        bad += 1
    print("RESULT:", "IPS matches patch.s" if not bad else "MISMATCH")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
