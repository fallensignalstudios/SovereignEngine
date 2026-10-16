#!/usr/bin/env python3
"""Prints a PNG's size, SHA-256 and tEXt provenance chunks (no engine code involved).
Usage: png_provenance.py image.png [more.png ...] [--json]"""
import hashlib, json, struct, sys


def read(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path}: not a PNG")
    info = {"file": path, "sha256": hashlib.sha256(data).hexdigest(), "text": {}}
    pos = 8
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            info["width"], info["height"] = struct.unpack(">II", body[:8])
        elif ctype == b"tEXt":
            key, _, value = body.partition(b"\0")
            info["text"][key.decode("latin-1")] = value.decode("latin-1")
        pos += 12 + length
    return info


def main(argv):
    as_json = "--json" in argv
    infos = [read(p) for p in argv if p != "--json"]
    if as_json:
        print(json.dumps(infos, indent=2))
        return
    for i in infos:
        print(f"{i['file']}: {i.get('width')}x{i.get('height')} sha256 {i['sha256']}")
        for k, v in i["text"].items():
            print(f"  {k}: {v}")


if __name__ == "__main__":
    main(sys.argv[1:])
