from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageOps


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets'
PNG_PATH = ASSETS / 'appicon.png'
ICO_PATH = ASSETS / 'appicon.ico'


def clamp(value: int) -> int:
    return max(0, min(255, value))


def rgba(hex_color: str, alpha: int = 255) -> tuple[int, int, int, int]:
    hex_color = hex_color.lstrip('#')
    return (
        int(hex_color[0:2], 16),
        int(hex_color[2:4], 16),
        int(hex_color[4:6], 16),
        alpha,
    )


def lerp(a: int, b: int, t: float) -> int:
    return int(round(a + (b - a) * t))


def mix(c0: tuple[int, int, int, int], c1: tuple[int, int, int, int], t: float) -> tuple[int, int, int, int]:
    return tuple(lerp(a, b, t) for a, b in zip(c0, c1))


def draw_concentric_ellipse(
    img: Image.Image,
    bbox: tuple[float, float, float, float],
    inner: tuple[int, int, int, int],
    outer: tuple[int, int, int, int],
    steps: int,
) -> None:
    draw = ImageDraw.Draw(img, 'RGBA')
    x0, y0, x1, y1 = bbox
    for i in range(steps):
        t = i / max(1, steps - 1)
        inset = t * min(x1 - x0, y1 - y0) * 0.5
        color = mix(outer, inner, t)
        draw.ellipse((x0 + inset, y0 + inset, x1 - inset, y1 - inset), fill=color)


def dashed_arc(
    draw: ImageDraw.ImageDraw,
    bbox: tuple[float, float, float, float],
    start: float,
    end: float,
    dash: float,
    gap: float,
    width: int,
    fill: tuple[int, int, int, int],
) -> None:
    angle = start
    while angle < end:
        seg_end = min(angle + dash, end)
        draw.arc(bbox, start=angle, end=seg_end, fill=fill, width=width)
        angle = seg_end + gap


def build_icon(size: int) -> Image.Image:
    img = Image.new('RGBA', (size, size), rgba('#0b0c10'))
    draw = ImageDraw.Draw(img, 'RGBA')

    panel = (size * 0.055, size * 0.055, size * 0.945, size * 0.945)
    draw.rounded_rectangle(panel, radius=int(size * 0.15), fill=rgba('#1b1e24'))

    outline = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    outline_draw = ImageDraw.Draw(outline, 'RGBA')
    outline_draw.rounded_rectangle(
        panel,
        radius=int(size * 0.15),
        outline=rgba('#5a5f67', 110),
        width=max(2, size // 128),
    )
    outline_draw.rounded_rectangle(
        (
            panel[0] + size * 0.01,
            panel[1] + size * 0.01,
            panel[2] - size * 0.01,
            panel[3] - size * 0.01,
        ),
        radius=int(size * 0.135),
        outline=rgba('#ffffff', 18),
        width=max(1, size // 256),
    )
    outline = outline.filter(ImageFilter.GaussianBlur(size / 512))
    img = Image.alpha_composite(img, outline)
    draw = ImageDraw.Draw(img, 'RGBA')

    shadow = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow, 'RGBA')
    shadow_draw.rounded_rectangle(
        (
            panel[0] + size * 0.02,
            panel[1] + size * 0.02,
            panel[2] - size * 0.005,
            panel[3] - size * 0.005,
        ),
        radius=int(size * 0.13),
        fill=rgba('#000000', 85),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(size / 128))
    img = Image.alpha_composite(img, shadow)
    draw = ImageDraw.Draw(img, 'RGBA')

    cx = cy = size / 2
    outer_r = size * 0.17
    inner_r = size * 0.145
    knob_box = (cx - outer_r, cy - outer_r, cx + outer_r, cy + outer_r)
    draw_concentric_ellipse(img, knob_box, rgba('#eef0f4'), rgba('#747a85'), 96)
    draw.ellipse(
        (cx - outer_r * 1.02, cy - outer_r * 1.02, cx + outer_r * 1.02, cy + outer_r * 1.02),
        outline=rgba('#f5f7fb', 220),
        width=max(4, size // 96),
    )
    draw.ellipse(
        (cx - outer_r * 0.87, cy - outer_r * 0.87, cx + outer_r * 0.87, cy + outer_r * 0.87),
        outline=rgba('#101216', 255),
        width=max(2, size // 128),
    )
    draw_concentric_ellipse(
        img,
        (cx - inner_r, cy - inner_r, cx + inner_r, cy + inner_r),
        rgba('#dddfe3'),
        rgba('#646a74'),
        72,
    )

    ring_r = size * 0.30
    ring_box = (cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r)
    cyan = rgba('#66dff6')
    draw.ellipse(ring_box, outline=cyan, width=max(5, size // 120))
    draw.line((cx, cy - ring_r * 1.15, cx, cy + ring_r * 1.15), fill=cyan, width=max(2, size // 180))
    draw.line((cx - ring_r * 1.15, cy, cx + ring_r * 1.15, cy), fill=cyan, width=max(2, size // 180))

    bracket = size * 0.065
    margin = size * 0.175
    lw = max(4, size // 150)
    for x, y, sx, sy in [
        (margin, margin, 1, 1),
        (size - margin, margin, -1, 1),
        (margin, size - margin, 1, -1),
        (size - margin, size - margin, -1, -1),
    ]:
        draw.line((x, y, x + sx * bracket, y), fill=cyan, width=lw)
        draw.line((x, y, x, y + sy * bracket), fill=cyan, width=lw)

    marker = size * 0.018
    for angle in [0, 90, 180, 270]:
        rad = math.radians(angle)
        px = cx + math.cos(rad) * ring_r
        py = cy - math.sin(rad) * ring_r
        box = (px - marker, py - marker, px + marker, py + marker)
        draw.rectangle(box, fill=cyan)
        draw.rectangle(
            (
                box[0] + marker * 0.35,
                box[1] + marker * 0.35,
                box[2] - marker * 0.35,
                box[3] - marker * 0.35,
            ),
            fill=rgba('#0f1320'),
        )

    purple = rgba('#9070ff')
    inner_ring_r = size * 0.23
    inner_box = (cx - inner_ring_r, cy - inner_ring_r, cx + inner_ring_r, cy + inner_ring_r)
    dashed_arc(draw, inner_box, 185, 350, dash=7, gap=7, width=max(5, size // 130), fill=purple)

    motion_box = (
        cx - inner_ring_r * 1.04,
        cy - inner_ring_r * 1.04,
        cx + inner_ring_r * 1.04,
        cy + inner_ring_r * 1.04,
    )
    draw.arc(motion_box, start=292, end=352, fill=purple, width=max(6, size // 115))

    handle = size * 0.018
    for px, py in [
        (cx + inner_ring_r * 0.78, cy + inner_ring_r * 0.63),
        (cx + inner_ring_r * 0.38, cy + inner_ring_r * 0.95),
    ]:
        draw.rectangle((px - handle, py - handle, px + handle, py + handle), fill=purple)
    draw.ellipse(
        (
            cx + inner_ring_r * 0.58 - handle * 0.9,
            cy + inner_ring_r * 0.82 - handle * 0.9,
            cx + inner_ring_r * 0.58 + handle * 0.9,
            cy + inner_ring_r * 0.82 + handle * 0.9,
        ),
        outline=purple,
        width=max(3, size // 180),
    )

    gloss = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    gloss_draw = ImageDraw.Draw(gloss, 'RGBA')
    gloss_draw.ellipse(
        (
            cx - outer_r * 0.78,
            cy - outer_r * 0.86,
            cx + outer_r * 0.45,
            cy + outer_r * 0.15,
        ),
        fill=rgba('#ffffff', 55),
    )
    gloss = gloss.filter(ImageFilter.GaussianBlur(size / 64))
    img = Image.alpha_composite(img, gloss)
    draw = ImageDraw.Draw(img, 'RGBA')

    noise = Image.effect_noise((size, size), 8).convert('L')
    noise = ImageOps.autocontrast(noise)
    noise = noise.point(lambda v: clamp(v // 3))
    grain = Image.merge('RGBA', [noise, noise, noise, noise.point(lambda v: v // 8)])
    img = Image.alpha_composite(img, grain)

    return img


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    base = build_icon(2048)

    png = base.resize((1024, 1024), Image.Resampling.LANCZOS)
    png.save(PNG_PATH)

    ico_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    ico_base = base.resize((256, 256), Image.Resampling.LANCZOS)
    ico_base.save(ICO_PATH, format='ICO', sizes=ico_sizes)


if __name__ == '__main__':
    main()
