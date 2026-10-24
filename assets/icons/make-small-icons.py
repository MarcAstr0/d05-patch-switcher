#!/usr/bin/env python3
"""Writes the hand-tuned small icons d05-patch-switcher-{16,24,32}.png.

docs/design/DESIGN.md section 14.1. The SVG rendered at these sizes blurs the dot
matrix into a green block, so they are placed pixel by pixel here instead: the same
tile, display window, "P11" and three buttons with the middle LED lit, in the same
tokens. Every element sits on whole pixels; only the rounded tile corners carry
partial alpha (16x supersampled coverage). Needs Pillow.

Usage: python3 -I make-small-icons.py [OUTDIR]   (default: this file's directory)
"""
import os
import sys

from PIL import Image

T = dict(panel='#202326', band='#26292B', bandEdge='#3A3E41', bezelRim='#2E3134',
         lcdBack='#071104', lcdOn='#98CF1C', buttonFace='#3F4244',
         ledOn='#F0383F', ledOff='#2A1A1A')

# Appendix A glyphs (5 x 7) for 24 and 32 px; a 3 x 5 cut of the same letters for 16 px.
P5 = ['####.', '#...#', '#...#', '####.', '#....', '#....', '#....']
ONE5 = ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.']
P3 = ['###', '#.#', '###', '#..', '#..']
ONE3 = ['.#', '##', '.#', '.#', '.#']


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


class Canvas:
    def __init__(self, n):
        self.n = n
        self.px = [[rgb(T['panel'])] * n for _ in range(n)]

    def rect(self, x, y, w, h, tok):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.px[yy][xx] = rgb(T[tok])

    def bits(self, x, y, rows, tok='lcdOn'):
        for r, row in enumerate(rows):
            for c, ch in enumerate(row):
                if ch == '#':
                    self.px[y + r][x + c] = rgb(T[tok])

    def save(self, path, radius, ss=16):
        n = self.n
        im = Image.new('RGBA', (n, n))
        for y in range(n):
            for x in range(n):
                hit = 0
                for sy in range(ss):
                    for sx in range(ss):
                        px, py = x + (sx + .5) / ss, y + (sy + .5) / ss
                        cx, cy = min(max(px, radius), n - radius), min(max(py, radius), n - radius)
                        hit += (px - cx) ** 2 + (py - cy) ** 2 <= radius * radius
                im.putpixel((x, y), self.px[y][x] + (round(255 * hit / ss / ss),))
        im.save(path, optimize=True)


def draw(n):
    c = Canvas(n)
    if n == 16:
        c.rect(1, 2, 14, 9, 'bezelRim'); c.rect(2, 3, 12, 7, 'lcdBack')
        c.bits(4, 4, P3); c.bits(8, 4, ONE3); c.bits(11, 4, ONE3)
        c.rect(0, 12, 16, 1, 'bandEdge'); c.rect(0, 13, 16, 3, 'band')
        faces, face_y, face_w, face_h, led_y = (1, 6, 11), 13, 4, 2, 13
        radius = 3
    elif n == 24:
        c.rect(2, 3, 20, 11, 'bezelRim'); c.rect(3, 4, 18, 9, 'lcdBack')
        for i, g in enumerate((P5, ONE5, ONE5)):
            c.bits(4 + 6 * i, 5, g)
        c.rect(0, 16, 24, 1, 'bandEdge'); c.rect(0, 17, 24, 7, 'band')
        faces, face_y, face_w, face_h, led_y = (4, 10, 16), 18, 4, 3, 19
        radius = 4
    elif n == 32:
        c.rect(4, 5, 24, 13, 'bezelRim'); c.rect(5, 6, 22, 11, 'lcdBack')
        for i, g in enumerate((P5, ONE5, ONE5)):
            c.bits(8 + 6 * i, 8, g)
        c.rect(0, 21, 32, 1, 'bandEdge'); c.rect(0, 22, 32, 10, 'band')
        faces, face_y, face_w, face_h, led_y = (5, 13, 21), 24, 6, 3, 25
        radius = 6
    for i, x in enumerate(faces):
        c.rect(x, face_y, face_w, face_h, 'buttonFace')
        c.rect(x + (face_w - 2) // 2, led_y, 2, 1, 'ledOn' if i == 1 else 'ledOff')
    return c, radius


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    for n in (16, 24, 32):
        c, radius = draw(n)
        c.save(os.path.join(out, 'd05-patch-switcher-%d.png' % n), radius)


if __name__ == '__main__':
    main()
