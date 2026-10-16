#!/usr/bin/env python3
"""Independent PNG check: decodes with Python's zlib (no engine code) and compares pixels.
Usage: png_decode_check.py image.png expected_rgba.raw   -> exit 0 if identical."""
import struct, sys, zlib

def decode(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "bad signature"
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4]); ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]; crc = struct.unpack(">I", data[pos + 8 + length:pos + 12 + length])[0]
        assert zlib.crc32(ctype + body) & 0xffffffff == crc, "bad CRC in " + ctype.decode()
        if ctype == b"IHDR":
            w, h, depth, color = struct.unpack(">IIBB", body[:10]); assert depth == 8 and color == 6
        elif ctype == b"IDAT":
            idat += body
        pos += 12 + length
    raw, stride, out, prev = zlib.decompress(idat), w * 4, bytearray(), bytearray(w * 4)
    for y in range(h):
        f, row = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = row[x - 4] if x >= 4 else 0; b = prev[x]; c = prev[x - 4] if x >= 4 else 0
            if f == 1: row[x] = (row[x] + a) & 255
            elif f == 2: row[x] = (row[x] + b) & 255
            elif f == 3: row[x] = (row[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += row; prev = row
    return bytes(out)

if __name__ == "__main__":
    sys.exit(0 if decode(sys.argv[1]) == open(sys.argv[2], "rb").read() else 1)
