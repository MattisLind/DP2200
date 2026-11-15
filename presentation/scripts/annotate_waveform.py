#!/usr/bin/env python3
"""Measure extrema in the original trace and draw an exact annotation diagram.

Run from the repository root:
PYTHONPATH=build/tools/python python3 presentation/scripts/annotate_waveform.py
"""
import base64
import argparse
from html import escape
import json
from pathlib import Path

import numpy as np
import pymupdf as fitz


ROOT = Path(__file__).resolve().parents[2]
FOLDER = ROOT / "presentation/Bilder"
SOURCE = FOLDER / "Tape-waveform.png"
OUTPUT = FOLDER / "Tape-waveform-annotated"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--remove-cursor', action='store_true')
    parser.add_argument('--output', default=OUTPUT.name, help='Output basename in Bilder (without extension)')
    args = parser.parse_args()
    output = FOLDER / args.output
    pix = fitz.Pixmap(str(SOURCE))
    pixels = np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width, pix.n).copy()
    original = pixels.copy()
    cursor_columns = []
    if args.remove_cursor:
        dark = pixels[:, :, :3].max(axis=2) < 80
        cursor_columns = np.flatnonzero(dark.mean(axis=0) > 0.8).tolist()
        assert cursor_columns == [931], 'Unexpected marker; review source before reconstructing'
        for x in cursor_columns:
            # Only this opaque cursor column is missing; use both adjacent
            # columns to reconstruct background, axis, and trace separately.
            neighbors = original[:, [x - 1, x + 1], :3]
            background = np.median(original[:, x - 1, :3], axis=0).astype(np.uint8)
            pixels[:, x, :3] = background
            blue_rows = []
            blue_colors = []
            for j in range(2):
                col = neighbors[:, j, :].astype(int)
                mask = (col[:, 2] - col[:, 0] > 40) & (col[:, 2] - col[:, 1] > 40)
                rows = np.flatnonzero(mask)
                assert rows.size
                blue_rows.append((rows[0], rows[-1]))
                blue_colors.extend(col[rows].tolist())
            low, high = np.rint(np.mean(blue_rows, axis=0)).astype(int)
            pixels[low:high + 1, x, :3] = np.median(blue_colors, axis=0).astype(np.uint8)
            axis = np.all(neighbors.max(axis=2) < 80, axis=1)
            pixels[axis, x, :3] = 0
        changed_columns = np.flatnonzero(np.any(pixels != original, axis=(0, 2))).tolist()
        assert changed_columns == cursor_columns
    background_pix = fitz.Pixmap(fitz.csRGB, pix.width, pix.height, pixels[:, :, :3].tobytes(), False)
    background_png = background_pix.tobytes('png')
    red, green, blue = (pixels[:, :, i].astype(int) for i in range(3))
    trace = (blue - red > 40) & (blue - green > 40)
    ys = np.array([
        np.median(np.flatnonzero(trace[:, x])) if trace[:, x].any() else np.nan
        for x in range(pix.width)
    ])
    valid = np.flatnonzero(np.isfinite(ys))
    ys = np.interp(np.arange(pix.width), valid, ys[valid])
    slopes = np.diff(ys)
    nonflat = np.flatnonzero(slopes)
    extrema = []
    for left, right in zip(nonflat, nonflat[1:]):
        if slopes[left] * slopes[right] < 0:
            # A flat extremum extends from left+1 through right inclusive.
            x = float(left + 1 + right) / 2
            extrema.append({"x": x, "y": float(ys[round(x)]),
                            "plateau": [int(left + 1), int(right)]})

    # One bit = two short intervals (0) or one long interval (1).
    indices = [0]
    bits = []
    i = 0
    while i + 1 < len(extrema):
        interval = extrema[i + 1]["x"] - extrema[i]["x"]
        if interval < 60:
            if i + 2 >= len(extrema):
                break
            assert extrema[i + 2]["x"] - extrema[i + 1]["x"] < 60
            bits.append("0")
            i += 2
        else:
            bits.append("1")
            i += 1
        indices.append(i)
    boundaries = [extrema[i]["x"] for i in indices]
    assert "".join(bits) == "001001101001", "Screenshot differs; review framing before annotating"
    assert "".join(bits[1:4]) == "010"
    assert len(bits[4:]) == 8

    width, height = pix.width, pix.height
    pdf = fitz.open()
    page = pdf.new_page(width=width, height=height)
    page.insert_image(page.rect, stream=background_png)
    encoded = base64.b64encode(background_png).decode()
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
           f'<image width="{width}" height="{height}" href="data:image/png;base64,{encoded}"/>']

    def color(rgb):
        return tuple(v / 255 for v in rgb)

    def rectangle(x0, y0, x1, y1, rgb, opacity):
        page.draw_rect(fitz.Rect(x0, y0, x1, y1), color=None, fill=color(rgb), fill_opacity=opacity)
        svg.append(f'<rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}" fill="rgb{rgb}" fill-opacity="{opacity}"/>')

    def line(x0, y0, x1, y1, dashed=False):
        page.draw_line((x0, y0), (x1, y1), color=(0.1, 0.1, 0.1), width=0.6,
                       dashes="[2 3] 0" if dashed else None)
        dash = ' stroke-dasharray="2 3"' if dashed else ''
        svg.append(f'<line x1="{x0}" y1="{y0}" x2="{x1}" y2="{y1}" stroke="#191919" stroke-width="0.6"{dash}/>')

    def text(center, baseline, value, size=16):
        text_width = fitz.get_text_length(value, fontname="hebo", fontsize=size)
        page.insert_text((center - text_width / 2, baseline), value, fontname="hebo", fontsize=size)
        svg.append(f'<text x="{center}" y="{baseline}" text-anchor="middle" font-family="Arial, Helvetica, sans-serif" font-size="{size}" font-weight="bold">{escape(value)}</text>')

    colors = {"0": (35, 90, 255), "1": (255, 45, 65)}
    for x0, x1, bit in zip(boundaries, boundaries[1:], bits):
        rectangle(x0, 100, x1, 390, colors[bit], 0.17)
        text((x0 + x1) / 2, 82, bit, 20)
    for x in boundaries:
        line(x, 100, x, 390, dashed=True)
    rectangle(280, 18, 310, 37, colors["0"], 0.25)
    text(390, 34, "0: full cycle", 18)
    rectangle(570, 18, 600, 37, colors["1"], 0.25)
    text(685, 34, "1: half cycle", 18)

    def bracket(x0, x1, label):
        line(x0, 399, x0, 409)
        line(x0, 409, x1, 409)
        line(x1, 409, x1, 399)
        text((x0 + x1) / 2, 432, label, 16)

    bracket(boundaries[1], boundaries[4], "Byte sync: 010")
    bracket(boundaries[4], boundaries[-1], "8 data bits: 01101001")
    svg.append('</svg>')
    output.with_suffix('.svg').write_text('\n'.join(svg))
    page.get_pixmap(matrix=fitz.Matrix(2, 2), alpha=False).save(str(output.with_suffix('.png')))
    report = {"source": SOURCE.name, "source_size": [width, height],
              "method": "Blue trace pixels; midpoint of each flat extremum plateau",
              "extrema": extrema, "bit_boundaries": boundaries, "bits": "".join(bits),
              "sync": "First 010 assumed as sync; remaining 8 bits are data",
              "reconstructed_cursor_columns": cursor_columns}
    output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Bit boundaries:', boundaries)
    print('Saved:', output.with_suffix('.png'))
    if cursor_columns:
        print('Only reconstructed source columns:', cursor_columns)


if __name__ == '__main__':
    main()
