#!/usr/bin/env python3
"""rplname.py -- fix up a wut-built RPL so the Cafe loader will accept it.
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


def undeflate_useless(data, headers):
    """Store raw any section that deflating did not actually shrink."""
    changed = []
    for i, h in enumerate(headers):
        if not (h[2] & SHF_DEFLATED) or h[4] == 0 or h[5] <= 4:
            continue
        try:
            raw = zlib.decompress(bytes(data[h[4] + 4:h[4] + h[5]]))
        except zlib.error:
            continue
        if len(raw) > h[5]:
            continue
        off, old = h[4], h[5]
        data[off:off + len(raw)] = raw
        # Blank whatever the shorter section no longer covers.
        data[off + len(raw):off + old] = b"\0" * (old - len(raw))
        h[2] &= ~SHF_DEFLATED
        h[5] = len(raw)
        changed.append((i, old, len(raw)))
    return changed


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

    undeflated = undeflate_useless(data, headers)

    # The file-info section is not the last thing in the file, so growing it in
    # place would push every section after it along -- which meant re-laying the
    # whole file, and a console loader that streams it refused the result. Move
    # the grown section to the end instead: every other byte stays exactly where
    # elf2rpl put it.
    fi_h = headers[fi]
    info = bytearray(data[fi_h[4]:fi_h[4] + FILEINFO_SIZE])
    encoded = name.encode("ascii") + b"\0"
    encoded += b"\0" * (align(len(encoded), 4) - len(encoded))
    struct.pack_into(">I", info, FILENAME_FIELD, FILEINFO_SIZE)
    blob = bytes(info) + encoded

    out = bytearray(data)
    pos = align(len(out), max(fi_h[8], 4) or 4)
    out += b"\0" * (pos - len(out))
    fi_h[4] = pos
    fi_h[5] = len(blob)
    out += blob

    # Its CRC, over the uncompressed bytes, into the table's slot for it.
    cr_h = headers[cr]
    struct.pack_into(">I", out, cr_h[4] + fi * 4, zlib.crc32(blob) & 0xFFFFFFFF)

    for i in [fi] + [c[0] for c in undeflated]:
        struct.pack_into(">IIIIIIIIII", out, shoff + i * shentsize, *headers[i])

    with open(path, "wb") as f:
        f.write(out)
    note = "".join(", section %d %d->%d raw" % c for c in undeflated)
    sys.stdout.write("%s: file-info name '%s' (%d bytes -> %d)%s\n"
                     % (path, name, len(data), len(out), note))
    return 0


if __name__ == "__main__":
    sys.exit(main())
