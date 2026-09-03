#!/usr/bin/env python3
"""rplname.py -- give a wut-built RPL the file-info name the loader lists it by.

    python rplname.py wwhd_cheats.rpl wwhd_cheats.rpl

Why this exists. When the Cafe loader registers a module, the name it reports
through OSDynLoad_GetRPLInfo is NOT the name it was acquired by: it is the
string the RPL's own file-info section points to with its `filename` field --
which is why a retail title's modules show up as build-machine paths such as
`bin\\ghs\\cafe\\cos\\pads\\vpad\\NDEBUG\\vpad.rpl`. wut's elf2rpl writes
`filename = 0` and no string at all, so every wut-built RPL is listed with a
NULL name. Anything that walks that list and builds a string from the name
without checking -- Aroma's FunctionPatcher module does exactly that for an
executable-by-address patch -- then crashes the title in strlen.

What it does. Appends `name` (NUL-terminated, padded to 4) to the file-info
section, sets `filename` to its offset from the start of that section,
recomputes the section's CRC32 in the CRC table (the loader checks it before
it will load the module), and rewrites the file with every section's data
re-laid-out after the section header table so the grown section fits. Nothing
else changes: compressed sections are copied as they are, and the only header
fields touched are offsets and sizes.

Layout facts this relies on, read from elf2rpl and decaf's loader: the
file-info and CRC sections are never deflated; the CRC table holds one
big-endian u32 per section in section order, computed over the section's
uncompressed data, with the CRC section's own entry zero; `filename` is an
offset relative to the file-info section's start; the file-info struct is
0x60 bytes with `filename` at +0x30.
"""

import struct
import sys
import zlib

SHT_NOBITS = 8
SHT_RPL_CRCS = 0x80000003
SHT_RPL_FILEINFO = 0x80000004
SHF_DEFLATED = 0x08000000
FILEINFO_SIZE = 0x60
FILENAME_FIELD = 0x30
DATA_ALIGN = 0x40


def align(n, a):
    return (n + a - 1) & ~(a - 1)


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: rplname.py <file.rpl> <name>\n")
        return 2
    path, name = sys.argv[1], sys.argv[2]
    with open(path, "rb") as f:
        data = bytearray(f.read())

    if data[:4] != b"\x7fELF" or data[5] != 2:
        sys.stderr.write("%s: not a big-endian ELF\n" % path)
        return 1
    ehsize = struct.unpack(">H", data[0x28:0x2A])[0]
    shoff = struct.unpack(">I", data[0x20:0x24])[0]
    shentsize, shnum = struct.unpack(">HH", data[0x2E:0x32])
    if shentsize != 0x28 or shnum == 0 or shoff == 0:
        sys.stderr.write("%s: unexpected section header table\n" % path)
        return 1

    # Section headers: name type flags addr offset size link info addralign entsize
    headers = []
    for i in range(shnum):
        o = shoff + i * shentsize
        headers.append(list(struct.unpack(">IIIIIIIIII", data[o:o + shentsize])))

    fi = [i for i, h in enumerate(headers) if h[1] == SHT_RPL_FILEINFO]
    cr = [i for i, h in enumerate(headers) if h[1] == SHT_RPL_CRCS]
    if len(fi) != 1 or len(cr) != 1:
        sys.stderr.write("%s: expected one file-info and one CRC section\n" % path)
        return 1
    fi, cr = fi[0], cr[0]
    if headers[fi][2] & SHF_DEFLATED or headers[cr][2] & SHF_DEFLATED:
        sys.stderr.write("%s: file-info or CRC section is deflated; not handled\n" % path)
        return 1
    if headers[fi][5] < FILEINFO_SIZE:
        sys.stderr.write("%s: file-info section is %d bytes, expected >= 0x60\n" % (path, headers[fi][5]))
        return 1
    if headers[cr][5] != shnum * 4:
        sys.stderr.write("%s: CRC table has %d entries, expected %d\n" % (path, headers[cr][5] // 4, shnum))
        return 1

    # Pull every section's data out, in file order, exactly as stored.
    blobs = {}
    for i, h in enumerate(headers):
        if h[1] == SHT_NOBITS or h[4] == 0 or h[5] == 0:
            continue
        blobs[i] = bytes(data[h[4]:h[4] + h[5]])

    # The file-info section: keep the struct, drop any previous name, add ours.
    info = bytearray(blobs[fi][:FILEINFO_SIZE])
    encoded = name.encode("ascii") + b"\0"
    encoded += b"\0" * (align(len(encoded), 4) - len(encoded))
    struct.pack_into(">I", info, FILENAME_FIELD, FILEINFO_SIZE)
    blobs[fi] = bytes(info) + encoded

    # Its CRC, over the uncompressed bytes, into the table's slot for it.
    crcs = bytearray(blobs[cr])
    struct.pack_into(">I", crcs, fi * 4, zlib.crc32(blobs[fi]) & 0xFFFFFFFF)
    blobs[cr] = bytes(crcs)

    # Re-lay the file: ELF header, section header table where it was, then
    # every section's data in its original file order, each 0x40-aligned.
    out = bytearray(data[:shoff + shnum * shentsize])
    order = sorted(blobs.keys(), key=lambda i: headers[i][4])
    for i in order:
        pos = align(len(out), DATA_ALIGN)
        out += b"\0" * (pos - len(out))
        headers[i][4] = pos
        headers[i][5] = len(blobs[i])
        out += blobs[i]
    for i, h in enumerate(headers):
        o = shoff + i * shentsize
        struct.pack_into(">IIIIIIIIII", out, o, *h)

    with open(path, "wb") as f:
        f.write(out)
    sys.stdout.write("%s: file-info name '%s' (%d bytes -> %d)\n" % (path, name, len(data), len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
