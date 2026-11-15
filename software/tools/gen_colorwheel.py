#!/usr/bin/env python3
"""Renders the Geckoconsole colour wheel to external_components/colorwheel.png.

Geometry must match gecko_console.h (WHEEL_R): hue is the angle (0 deg = red at the right,
clockwise on screen, like Home Assistant's colour wheel), saturation the distance from the centre, value 1. Pixels
outside the circle get the UI background colour. ESPHome stores the image as RGB565 in flash,
so the console only looks pixels up instead of computing them.

Usage: python3 tools/gen_colorwheel.py   (from the software/ directory)
"""

import colorsys
import math
from pathlib import Path

from PIL import Image

R = 80  # WHEEL_R in gecko_console.h
BG = (8, 12, 16)  # BG in gecko_console.h

size = 2 * R + 1
img = Image.new("RGB", (size, size), BG)
px = img.load()
for y in range(size):
    dy = y - R  # screen y points down: hue grows clockwise
    for x in range(size):
        dx = x - R
        r2 = dx * dx + dy * dy
        if r2 > R * R:
            continue
        hue = math.degrees(math.atan2(dy, dx)) % 360.0
        r, g, b = colorsys.hsv_to_rgb(hue / 360.0, math.sqrt(r2) / R, 1.0)
        px[x, y] = (round(r * 255), round(g * 255), round(b * 255))

out = Path(__file__).resolve().parent.parent / "external_components" / "colorwheel.png"
img.save(out)
print(f"wrote {out} ({size}x{size})")
