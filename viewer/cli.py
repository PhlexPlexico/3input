#!/usr/bin/env python3
"""Terminal client for 3Input, needs nothing but Python (no tkinter).

    python3 cli.py <3ds ip> [--raw]

Shows a small block of live values that is redrawn in place and wraps to the
width of the terminal, including how the ZL/ZR/C-Stick paths are doing. With
--raw (or when output is not a terminal) one unwrapped line is printed per packet.
"""
import shutil
import sys
import textwrap
import time

from threeinput import BTN, Conn, IR_STATUS, CS_STATUS

PORT = 65534
MIN_WIDTH = 20


def raw_line(j):
    """Everything on a single line, for logs and pipes."""
    held = " ".join(b.name for b in BTN if j[b]) or "-"
    return (f"seq={j['seq']} cp=({j['cp_x']},{j['cp_y']}) tp=({j['tp_x']},{j['tp_y']}) "
            f"cs=({j['cs_x']},{j['cs_y']}) {held} "
            f"[IR: {IR_STATUS.get(j['ir_status'], j['ir_status'])}, 0x{j['ir_error']:08X}] "
            f"[CS: {CS_STATUS.get(j['cs_status'], j['cs_status'])}, 0x{j['cs_error']:08X}]")


def format_block(j, width):
    """The values as lines no longer than `width - 1`, so they never wrap by themselves."""
    width = max(MIN_WIDTH, width) - 1
    held = " ".join(b.name for b in BTN if j[b]) or "-"
    # (text, indent for continuation lines). Values contain no spaces, so they are never split.
    logical = [
        (f"seq={j['seq']} cp=({j['cp_x']},{j['cp_y']}) tp=({j['tp_x']},{j['tp_y']}) cs=({j['cs_x']},{j['cs_y']})", ""),
        (f"buttons: {held}", "  "),
        (f"IR: {IR_STATUS.get(j['ir_status'], j['ir_status'])} (0x{j['ir_error']:08X})", "    "),
        (f"CS: {CS_STATUS.get(j['cs_status'], j['cs_status'])} (0x{j['cs_error']:08X})", "    "),
    ]
    lines = []
    for text, indent in logical:
        lines += textwrap.wrap(text, width=width, subsequent_indent=indent,
                               break_long_words=True, break_on_hyphens=False) or [""]
    return lines


class Display:
    """Redraws a block of lines in place, following the terminal width."""

    def __init__(self):
        self.prev_lines = 0
        self.prev_width = 0

    def show(self, j):
        width = shutil.get_terminal_size((80, 24)).columns
        lines = format_block(j, width)
        out = []
        if width != self.prev_width:
            out.append("\x1b[2J\x1b[H")  # resized: old lines may have been reflowed, start clean
        elif self.prev_lines:
            out.append(f"\x1b[{self.prev_lines}A")
        out.append("".join("\r\x1b[2K" + l + "\n" for l in lines))
        out.append("\x1b[J")  # drop leftovers if the block got shorter
        sys.stdout.write("".join(out))
        sys.stdout.flush()
        self.prev_lines, self.prev_width = len(lines), width


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        sys.exit(__doc__)
    raw = "--raw" in sys.argv or not sys.stdout.isatty()
    conn = Conn(args[0], PORT)
    display = Display()
    try:
        while conn.keepGoing:
            if not conn.hasInput():
                time.sleep(0.001)
                continue
            j = conn.getInput()
            if raw:
                print(raw_line(j))
            else:
                display.show(j)
    except KeyboardInterrupt:
        pass
    finally:
        conn.stop()
        conn.join()


if __name__ == "__main__":
    main()
