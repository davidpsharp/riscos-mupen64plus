#!/usr/bin/env python3
# RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
"""riscos_zip.py - a zip of a RISC OS application that unpacks with its file types (docs/RISCOS_BUILD.md).

    riscos_zip.py OUT.zip MANIFEST

MANIFEST lists one file per line: "TYPE  path/inside/the/zip  source/file/on/this/machine", TYPE the three hex digits of the
RISC OS file type (ff8 absolute, ffb BASIC, fea Obey is feb, ffd data, fff text, ff9 sprite, ffa module, fec template ...); '#'
starts a comment.  Paths use '/' between directories as zip files do, and RISC OS's '.' in a leaf is written as it is (a RISC OS
unzip makes it '/').

How the types travel.  Each entry carries Acorn's "ARC0" extra field (header id 0x4341, as SparkFS and Info-ZIP's RISC OS unzip
write and read): the load and execution addresses, whose top twelve bits (&FFF) and the next twelve say "this file has a type and
a date" and give the type, and the attributes.  The names stay plain (no ",xxx"), and the entries say they were made on an Acorn
(the "version made by" host byte 13).  Tools that know nothing of the extra field (unzip on a Mac or Linux) unpack the same files
without types; on RISC OS, SparkFS, Info-ZIP's UnZip and !SparkPlug restore them, and a file type can always be set again with
*SetType.  The date stamped is the one given by SOURCE_DATE_EPOCH (default: 2026-10-13 00:00:00 UTC), so that the zip is the same
every time it is made.
"""
import os
import struct
import sys
import time
import zipfile

CENTISECONDS_1900_TO_1970 = 2208988800 * 100


def acorn_extra(filetype, epoch):
    cs = epoch * 100 + CENTISECONDS_1900_TO_1970          # centiseconds since 1900, five bytes
    load = 0xFFF00000 | (filetype << 8) | ((cs >> 32) & 0xFF)
    exec_ = cs & 0xFFFFFFFF
    attr = 0x33                                            # owner read and write, public read and write
    body = b"ARC0" + struct.pack("<IIII", load, exec_, attr, 0)
    return struct.pack("<HH", 0x4341, len(body)) + body


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    out, manifest = sys.argv[1], sys.argv[2]
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "1791849600"))      # 2026-10-13
    stamp = time.gmtime(epoch)[:6]
    entries = []
    for line in open(manifest):
        line = line.split("#")[0].strip()
        if line:
            ftype, name, src = line.split(None, 2)
            entries.append((int(ftype, 16), name, src))
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        dirs = set()
        for _, name, _ in entries:
            parts = name.split("/")[:-1]
            for i in range(1, len(parts) + 1):
                dirs.add("/".join(parts[:i]))
        for d in sorted(dirs):
            info = zipfile.ZipInfo(d + "/", stamp)
            info.create_system = 13
            info.external_attr = 0o40755 << 16
            z.writestr(info, b"")
        for ftype, name, src in entries:
            info = zipfile.ZipInfo(name, stamp)
            info.create_system = 13
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            info.extra = acorn_extra(ftype, epoch)
            with open(src, "rb") as f:
                z.writestr(info, f.read())
    print(f"{out}: {len(entries)} files")


if __name__ == "__main__":
    main()
