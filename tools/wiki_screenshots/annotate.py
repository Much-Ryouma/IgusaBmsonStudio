"""Draw numbered callouts on the screenshots written by ibs_wiki_screenshots.

usage: python annotate.py <shots folder> <wiki images folder>
"""

import json
import shutil
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ACCENT = (232, 83, 63)
FONT_CANDIDATES = [
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "C:/Windows/Fonts/meiryob.ttc",
    "C:/Windows/Fonts/arialbd.ttf",
]

CALLOUTS = {
    "main_arranger": [
        "menubar", "toolbar", "tabs", "arranger_header", "ruler", "placement",
        "arranger_scroll", "waveform", "waveform_scroll", "slicer_panel", "statusbar",
    ],
    "main_chart": [
        "header", "measures", "timing", "lanes", "bgm_columns", "bga_columns",
        "asset_columns", "density", "minimap", "bga_panel", "statusbar",
    ],
    "slicer_preview": [
        "param:検出方法", "param:感度", "param:最小間隔", "param:無音ゲート", "param:プリロール",
        "param:クオンタイズ", "param:強さ", "param:寄せる範囲", "button:検出", "button:適用", "status",
    ],
}

ANCHORS = {
    "menubar": "right", "toolbar": "right", "statusbar": "center", "tabs": "after",
    "arranger_header": "bottom", "ruler": "center", "placement": "center", "waveform": "inside_low",
    "slicer_panel": "before", "arranger_scroll": "center", "waveform_scroll": "center",
    "header": "header", "measures": "bottom", "timing": "bottom", "lanes": "bottom", "bgm_columns": "bottom",
    "bga_columns": "bottom", "asset_columns": "bottom", "density": "bottom", "minimap": "bottom", "bga_panel": "center",
}

COPIED = [
    "slicer_cuts", "about", "new_chart", "song_info", "song_info_images", "settings",
    "statistics", "nulltest", "menu_0", "menu_1", "menu_2", "menu_3", "menu_4", "menu_5", "menu_6",
]


def load_font(size):
    for candidate in FONT_CANDIDATES:
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def badge(draw, center, number, font):
    radius = 13
    x, y = center
    draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=ACCENT, outline=(255, 255, 255), width=2)
    text = str(number)
    box = draw.textbbox((0, 0), text, font=font)
    draw.text((x - (box[2] - box[0]) / 2 - box[0], y - (box[3] - box[1]) / 2 - box[1]), text, fill=(255, 255, 255), font=font)


def annotate(source, regions, order, destination):
    image = Image.open(source).convert("RGB")
    draw = ImageDraw.Draw(image)
    font = load_font(15)
    for number, key in enumerate(order, start=1):
        if key not in regions:
            print(f"warning: {key} not found in {source.name}")
            continue
        x, y, width, height = regions[key]
        if ANCHORS.get(key, "before") == "before":
            x, y, width, height = x - 3, y - 2, width + 6, height + 4
        draw.rectangle((x + 1, y + 1, x + width - 2, y + height - 2), outline=ACCENT, width=3)
        anchor = ANCHORS.get(key, "before")
        positions = {
            "right": (x + width - 18, y + height / 2),
            "center": (x + width / 2, y + height / 2),
            "after": (x + width + 18, y + height / 2),
            "before": (x - 16, y + height / 2),
            "bottom": (x + width / 2, y + height - 18),
            "inside_low": (x + 18, y + 40),
            "header": (x + width * 0.6, y + height / 2),
        }
        center = positions[anchor]
        center = (min(max(center[0], 15), image.width - 15), min(max(center[1], 15), image.height - 15))
        badge(draw, center, number, font)
    image.save(destination)


def main():
    shots = Path(sys.argv[1])
    images = Path(sys.argv[2])
    images.mkdir(parents=True, exist_ok=True)
    regions = json.loads((shots / "regions.json").read_text(encoding="utf-8"))
    for name, order in CALLOUTS.items():
        annotate(shots / f"{name}.png", regions[name], order, images / f"{name}.png")
    for name in COPIED:
        shutil.copyfile(shots / f"{name}.png", images / f"{name}.png")


if __name__ == "__main__":
    main()
