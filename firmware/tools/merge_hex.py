#!/usr/bin/env python3
"""Merge Bootloader.hex + App.hex into a single combined.hex.

Intel HEX files are plain text; both images sit in flash (BL @ 0x08000000,
App @ 0x08001000) with non-overlapping address ranges, so a record-by-record
concatenation (stripping the End record from each, appending one at the end)
produces a valid combined file. Type 04 (extended linear address) records
are kept from both — they re-state the same 0x0800 base, which is legal.

Usage: merge_hex.py <bl.hex> <app.hex> <combined.hex>
"""
import sys


def strip_end(records):
    end = ":00000001FF"
    return [r for r in records if r.strip() and r.strip() != end]


def main(bl_path, app_path, out_path):
    with open(bl_path, "r") as f:
        bl = f.read().splitlines()
    with open(app_path, "r") as f:
        app = f.read().splitlines()

    combined = strip_end(bl) + strip_end(app) + [":00000001FF"]

    with open(out_path, "w") as f:
        f.write("\n".join(combined) + "\n")

    print(f"merged {bl_path} + {app_path} -> {out_path} ({len(combined)} records)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("usage: merge_hex.py <bl.hex> <app.hex> <combined.hex>", file=sys.stderr)
        sys.exit(1)
    main(sys.argv[1], sys.argv[2], sys.argv[3])
