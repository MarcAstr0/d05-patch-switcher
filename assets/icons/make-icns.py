#!/usr/bin/env python3
"""Writes d05-patch-switcher.icns with the full PNG entry set iconutil writes.

Standard library only. Each entry is a PNG file stored as is (PNG entries are read by
macOS 10.7 and later; the app needs 12.0):

  icp4 16    icp5 32    ic07 128   ic08 256   ic09 512
  ic11 32 (16@2x)   ic12 64 (32@2x)   ic13 256 (128@2x)   ic14 512 (256@2x)   ic10 1024 (512@2x)

Usage: python3 -I make-icns.py OUT.icns DIR
DIR holds icon-16.png, icon-32.png, icon-64.png, icon-128.png, icon-256.png,
icon-512.png and icon-1024.png; each PNG's size is checked against its entry.
"""
import os
import struct
import sys

ENTRIES = [('icp4', 16), ('icp5', 32), ('ic07', 128), ('ic08', 256), ('ic09', 512),
           ('ic11', 32), ('ic12', 64), ('ic13', 256), ('ic14', 512), ('ic10', 1024)]


def png_size(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
        raise ValueError('not a PNG')
    return struct.unpack('>II', data[16:24])


def main():
    out, src = sys.argv[1], sys.argv[2]
    body = b''
    for kind, px in ENTRIES:
        with open(os.path.join(src, 'icon-%d.png' % px), 'rb') as f:
            data = f.read()
        if png_size(data) != (px, px):
            raise SystemExit('icon-%d.png is %dx%d' % ((px,) + png_size(data)))
        body += kind.encode('ascii') + struct.pack('>I', len(data) + 8) + data
    with open(out, 'wb') as f:
        f.write(b'icns' + struct.pack('>I', len(body) + 8) + body)


if __name__ == '__main__':
    main()
