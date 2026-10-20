"""Generate only the monochrome home-screen artwork, inside this project.

Requires Pillow. The reuse package contributes only the brand artwork;
all status fields, channel readings and counters remain runtime data.
"""
from argparse import ArgumentParser
from collections import deque
from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "tools/ui_assets/sunwing_source.png"
OUTPUT = ROOT / "CH584_V1_0_1/APP/include/ui_home_assets.h"
LOGO_SOURCE = ROOT / "tools/ui_assets/sanwei_logo/ui_sanwei_logo.h"
BRAND_REFERENCE = ROOT / "tools/ui_assets/sanwei_logo/sunwing_hd_reference.png"
SCALE = 4
META_HEIGHT = 12
META_BASELINE = 10


def font(path, size):
    return ImageFont.truetype(str(path), size * SCALE)


def mono(image, threshold=160):
    return image.point(lambda value: 255 if value >= threshold else 0, mode="1")


def native_text(text, path, size, spacing=0):
    """Rasterize small brand text at its final size with monochrome hinting.

    Crop visible ink rather than the nominal font box. Individual advances
    preserve the glyphs and give the small italic letters a clear separation.
    """
    face = ImageFont.truetype(str(path), size)
    width = math.ceil(sum(face.getlength(char) + spacing for char in text)) + 2 * size
    canvas = Image.new("L", (width, size * 3), 0)
    draw = ImageDraw.Draw(canvas)
    draw.fontmode = "1"
    x = size
    for char in text:
        draw.text((x, size * 2), char, font=face, anchor="ls", fill=255)
        x += face.getlength(char) + spacing
    bounds = canvas.getbbox()
    if bounds is None:
        raise ValueError(f"Brand text has no visible ink: {text}")
    return canvas.crop(bounds)


def brand_bitmap(lishu_font, english_font):
    brand = Image.new("L", (60, 36), 0)
    # Keep only the original symbol; never shrink the reference's Latin word.
    source = Image.open(ART).convert("L").crop((0, 0, 224, 80))
    mark = Image.eval(source, lambda value: 255 - value)
    mark = mark.crop(mono(mark, 128).getbbox())
    english = native_text("sunwing", english_font, 11, spacing=1)
    name = native_text("三为矿安", lishu_font, 13)
    mark_height = brand.height - english.height - name.height - 2
    if mark_height <= 0 or max(english.width, name.width) > brand.width:
        raise ValueError("Brand fonts exceed the existing 60x36 home-screen area")
    mark_width = round(mark.width * mark_height / mark.height)
    if mark_width > brand.width:
        raise ValueError("Brand symbol exceeds the existing home-screen area")
    mark = mono(mark.resize((mark_width, mark_height), Image.Resampling.LANCZOS), 140)
    y = 0
    for part in (mark, english, name):
        brand.paste(part, ((brand.width - part.width) // 2, y))
        y += part.height + 1
    return mono(brand)


def read_reuse_logo(source_header):
    """Decode the immutable reuse-package XBMP; 1 is foreground ink."""
    import re

    text = source_header.read_text(encoding="utf-8")
    match = re.search(r"sanwei_logo_xbmp\[480\]\s*=\s*\{(.*?)\};", text, re.S)
    if match is None:
        raise ValueError("Expected the supplied 78x48, 480-byte Sanwei logo")
    data = bytes(int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]{2}\b", match.group(1)))
    if len(data) != 480 or any(data[row * 10 + 9] & 0xc0 for row in range(48)):
        raise ValueError("Invalid Sanwei XBMP length or row padding")
    source = Image.new("L", (78, 48), 0)
    for y in range(source.height):
        for x in range(source.width):
            if data[y * 10 + x // 8] & (1 << (x % 8)):
                source.putpixel((x, y), 255)
    return source


def reused_brand_bitmap(source_header):
    """Retain the previous 60x36 adaptation as an explicit source option."""
    source = read_reuse_logo(source_header)

    # Keep the original symbol, custom Sunwing wordmark and LiSu caption.
    # Resize each layer proportionally, with two clear rows between layers.
    brand = Image.new("L", (60, 36), 0)
    for box, size, y in (
        ((22, 0, 56, 18), (26, 14), 0),
        ((3, 21, 75, 30), (56, 7), 16),
        ((0, 34, 78, 48), (60, 11), 25),
    ):
        part = source.crop(box).resize(size, Image.Resampling.LANCZOS)
        brand.paste(mono(part, 128), ((brand.width - size[0]) // 2, y))
    return mono(brand)


def wordmark_components(word):
    """Separate original letters, including the dot and body of the italic i.

    Their horizontal bounds overlap, so splitting at blank columns would
    merge w/i. This runs only in the offline asset generator.
    """
    remaining = {(x, y) for y in range(word.height) for x in range(word.width)
                 if word.getpixel((x, y))}
    components = []
    while remaining:
        first = remaining.pop()
        pending = deque([first])
        pixels = [first]
        while pending:
            x, y = pending.popleft()
            for point in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if point in remaining:
                    remaining.remove(point)
                    pending.append(point)
                    pixels.append(point)
        if len(pixels) > 50:
            components.append(pixels)
    components.sort(key=lambda pixels: min(x for x, y in pixels))
    if len(components) != 8:
        raise ValueError("Expected Sunwing's seven letters and separate i dot")
    return components


def component_bitmap(pixels, line_height):
    left = min(x for x, y in pixels)
    right = max(x for x, y in pixels) + 1
    image = Image.new("L", (right - left, line_height), 0)
    for x, y in pixels:
        image.putpixel((x - left, y), 255)
    return image


def high_resolution_brand_bitmap(reference):
    """Sample the supplied original directly, preserving glyph separation."""
    with Image.open(reference) as image:
        if image.size != (920, 498):
            raise ValueError("Brand crops require the supplied 920x498 original")
        original = image.convert("L")
    # Exclude the registration symbol and the image's bottom border.
    mark = mono(Image.eval(original.crop((180, 30, 715, 367)), lambda v: 255 - v), 128)
    word = mono(Image.eval(original.crop((0, 380, 920, 490)), lambda v: 255 - v), 128)
    if mark.getbbox() is None or word.getbbox() is None:
        raise ValueError("Brand reference has no visible symbol or wordmark")
    mark = mark.crop(mark.getbbox()).convert("L")
    word = word.crop(word.getbbox()).convert("L")
    parts = wordmark_components(word)
    letters = parts[:4] + [parts[4] + parts[5]] + parts[6:]
    english = Image.new("L", (87, 10), 0)
    x = 0
    for index, (pixels, width) in enumerate(zip(letters, (10, 12, 12, 16, 5, 13, 13))):
        glyph = component_bitmap(pixels, word.height)
        glyph = mono(glyph.resize((width, 10), Image.Resampling.LANCZOS), 160)
        if index == 4:
            # One blank row below the i dot; keep its body on the common baseline.
            glyph = Image.new("L", (width, 10), 0)
            body = component_bitmap(parts[4], word.height)
            dot = component_bitmap(parts[5], word.height)
            body = mono(body.crop(body.getbbox()).resize((width, 6), Image.Resampling.LANCZOS), 160)
            dot = mono(dot.crop(dot.getbbox()).resize((3, 2), Image.Resampling.LANCZOS), 160)
            glyph.paste(body, (0, 3))
            glyph.paste(dot, (1, 0))
        english.paste(glyph, (x, 0))
        x += width + 1

    brand = Image.new("L", (88, 44), 0)
    symbol = mono(mark.resize((34, 18), Image.Resampling.LANCZOS), 140)
    brand.paste(symbol, (27, 0))
    brand.paste(english, (0, 20))
    # Caption still comes from the supplied artwork, enlarged with the brand.
    caption = read_reuse_logo(LOGO_SOURCE).crop((0, 34, 78, 48))
    caption = mono(caption.resize((74, 14), Image.Resampling.LANCZOS), 128)
    brand.paste(caption, (7, 30))
    return mono(brand)


def pack(image):
    """u8g2 XBMP: rows, ceil(width / 8) bytes, least significant bit first."""
    stride = (image.width + 7) // 8
    data = bytearray(stride * image.height)
    for y in range(image.height):
        for x in range(image.width):
            if image.getpixel((x, y)):
                data[y * stride + x // 8] |= 1 << (x % 8)
    return data


def array(name, image):
    data = pack(image)
    rows = ["    " + ", ".join(f"0x{x:02x}" for x in data[i:i + 16])
            for i in range(0, len(data), 16)]
    return (f"#define {name.upper()}_WIDTH {image.width}u\n"
            f"#define {name.upper()}_HEIGHT {image.height}u\n"
            f"static const uint8_t {name}[] = {{\n" + ",\n".join(rows) + "\n};\n")


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path,
                        help="First run: supplied 1672x941 layout image containing the brand")
    parser.add_argument("--bold-font", type=Path, default=Path("C:/Windows/Fonts/msyhbd.ttc"))
    parser.add_argument("--lishu-font", type=Path, default=Path("C:/Windows/Fonts/simli.ttf"))
    parser.add_argument("--english-font", type=Path, default=Path("C:/Windows/Fonts/arialbi.ttf"))
    parser.add_argument("--logo-source", type=Path,
                        help="Explicitly use the previous 60x36 adaptation of a supplied 78x48 XBMP header")
    parser.add_argument("--brand-reference", type=Path, default=BRAND_REFERENCE,
                        help="High-resolution 920x498 Sunwing original used by default")
    parser.add_argument("--legacy-brand", action="store_true",
                        help="Use the earlier reference/font brand generator instead of the reuse package")
    args = parser.parse_args()
    if args.reference:
        source = Image.open(args.reference)
        if source.size != (1672, 941):
            raise ValueError("Reference crop is defined for the supplied 1672x941 image")
        ART.parent.mkdir(parents=True, exist_ok=True)
        source.crop((76, 91, 300, 202)).convert("L").save(ART)

    if args.legacy_brand or args.reference:
        brand = brand_bitmap(args.lishu_font, args.english_font)
    elif args.logo_source:
        brand = reused_brand_bitmap(args.logo_source)
    else:
        brand = high_resolution_brand_bitmap(args.brand_reference)

    title = Image.new("L", (160 * SCALE, 21 * SCALE), 0)
    ImageDraw.Draw(title).text((0, 0), "矿用巷道综合测站", font=font(args.bold_font, 20),
                               anchor="lt", fill=255)
    title = mono(title.resize((160, 21), Image.Resampling.LANCZOS))

    chars = sorted(set("状态：开机关本号分站无中继0123456789.->V "))
    glyphs = []
    meta_font = font(args.bold_font, 11)
    for char in chars:
        width = math.ceil(meta_font.getlength(char) / SCALE)
        canvas = Image.new("L", (width * SCALE, META_HEIGHT * SCALE), 0)
        ImageDraw.Draw(canvas).text((0, META_BASELINE * SCALE), char,
                                     font=meta_font, anchor="ls", fill=255)
        bitmap = pack(mono(canvas.resize((width, META_HEIGHT), Image.Resampling.LANCZOS), 140))
        if len(bitmap) > 24:
            raise ValueError(f"Oversize metadata glyph: {char}")
        glyphs.append(f"    {{0x{ord(char):04x}u, {width}u, {{" +
                      ", ".join(f"0x{value:02x}" for value in bitmap) + "}}")

    labels = ["锚杆", "激光", "位移", "裂缝", "倾角", "应力", "液位", "微震", "地音", "测试"]
    label_arrays = []
    for index, label in enumerate(labels):
        canvas = Image.new("L", (28 * SCALE, 15 * SCALE), 0)
        ImageDraw.Draw(canvas).text((0, 0), label, font=font(args.bold_font, 14),
                                     anchor="lt", fill=255)
        label_arrays.append(array(f"ui_home_label_{index}",
                                  mono(canvas.resize((28, 15), Image.Resampling.LANCZOS), 150)))
    label_table = ("typedef struct { const char *text; const uint8_t *bits; } ui_home_label_t;\n"
                   "static const ui_home_label_t ui_home_labels[] = {\n" +
                   ",\n".join(f'    {{"{label}", ui_home_label_{index}}}'
                              for index, label in enumerate(labels)) + "\n};\n")

    text = ("/* Generated by tools/generate_home_assets.py; do not edit the byte arrays.\n"
            " * XBMP bits are ink=1. Runtime data and shared menu fonts are not included. */\n"
            "#ifndef UI_HOME_ASSETS_H\n#define UI_HOME_ASSETS_H\n\n#include <stdint.h>\n\n" +
            array("ui_home_brand", brand) + "\n" + array("ui_home_title", title) +
            f"\n#define UI_HOME_META_HEIGHT {META_HEIGHT}u\n"
            f"#define UI_HOME_META_BASELINE {META_BASELINE}u\n"
            "typedef struct { uint16_t code; uint8_t width; uint8_t bits[24]; } ui_home_glyph_t;\n"
            "static const ui_home_glyph_t ui_home_meta_glyphs[] = {\n" +
            ",\n".join(glyphs) + "\n};\n\n" +
            "\n".join(label_arrays) + "\n" + label_table + "\n#endif\n")
    OUTPUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"Generated {OUTPUT.relative_to(ROOT)}: brand {brand.size}, title {title.size}, "
          f"{len(glyphs)} metadata glyphs")


if __name__ == "__main__":
    main()
