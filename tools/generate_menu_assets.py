"""Generate menu-only glyphs and icons from the supplied UI reference.

All inputs and outputs belong to the current project. No runtime readings,
menu numbers or device settings are part of the artwork.
"""
from argparse import ArgumentParser
from pathlib import Path
import hashlib
import re

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "tools/ui_reference/chumaoli/二级页面"
RETURN_REFERENCE = ROOT / "tools/ui_reference/return_home/返回主页_3倍.png"
OUTPUT = ROOT / "CH584_V1_0_1/APP/include/ui_menu_assets.h"
UI_SOURCE = ROOT / "CH584_V1_0_1/APP/yuying_TFT.c"
TEXT = """地址分区组网测试设备绑定安装调试上传设置其他信息汇总返回主页
分站号本机保存并重启不开始重置已的名称一键目前扫描到蓝牙总数
错误警报电压异常恢复出厂确认功率通信亮屏时间状态开秒中继无更改为
请按键名称信号解绑个次序成功比率存通道电压精确稳定可靠安全相伴下一页台：:?"""
REFERENCE_NAMES = ["01_地址分区.png", "02_组网测试.png", "03_探头标定.png",
                   "04_安装调试.png", "05_上传设置.png", "06_其它设置.png",
                   "07_初锚力值.png", "08_返回主页.png"]


def pack(image):
    stride = (image.width + 7) // 8
    bits = bytearray(stride * image.height)
    for y in range(image.height):
        for x in range(image.width):
            if image.getpixel((x, y)):
                bits[y * stride + x // 8] |= 1 << (x % 8)
    return bits


def array(name, image):
    bits = pack(image)
    lines = ["    " + ", ".join(f"0x{value:02x}" for value in bits[i:i + 16])
             for i in range(0, len(bits), 16)]
    return f"static const uint8_t {name}[] = {{\n" + ",\n".join(lines) + "\n};\n"


def glyph(char, path, size, stroke_width=0, threshold=140):
    scale = 4
    face = ImageFont.truetype(str(path), size * scale)
    advance = round(face.getlength(char) / scale)
    canvas = Image.new("L", (advance * scale, (size + 2) * scale), 0)
    baseline = (size - 1) * scale
    if char == "_":
        # Keep the below-baseline underscore inside the existing glyph canvas.
        baseline = min(baseline, canvas.height - face.getbbox(char, anchor="ls")[3])
    ImageDraw.Draw(canvas).text((0, baseline), char, anchor="ls",
                               font=face, fill=255, stroke_width=stroke_width)
    image = canvas.resize((advance, size + 2), Image.Resampling.LANCZOS)
    image = image.point(lambda pixel: 255 if pixel >= threshold else 0, mode="1")
    if 32 < ord(char) < 127 and image.getbbox() is None:
        raise ValueError(f"Printable ASCII glyph is empty: {char!r} at {size}px")
    return image


def source_characters():
    """Collect UI literals without mistaking comments for rendered text."""
    source = UI_SOURCE.read_text(encoding="utf-8")
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*', source, re.S)
    literals = "".join(token for token in tokens if token.startswith('"'))
    return set(re.findall(r"[\u3400-\u9fff\uff00-\uffef]", literals))


def warning_glyph(path, size):
    """Use the same UI font with native monochrome hinting for dense strokes."""
    image = Image.new("L", (size, size + 2), 0)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    draw.text((0, size - 1), "警", anchor="ls",
              font=ImageFont.truetype(str(path), size), fill=255)
    original = image.convert("1")
    image = original.copy()
    # Add one pixel to the right of stems, while keeping narrow internal
    # counters open. Do not thicken vertically: lower horizontal gaps are 1px.
    for y in range(original.height):
        for x in range(1, original.width):
            if original.getpixel((x, y)) or not original.getpixel((x - 1, y)):
                continue
            if x + 1 < original.width and original.getpixel((x + 1, y)):
                continue
            image.putpixel((x, y), 255)
    return image


def return_lishu_glyph(char, path):
    """One size and weight for all three return-page captions."""
    if char in ("精", "靠"):
        return hinted_glyph(char, path, 22)
    return glyph(char, path, 22, stroke_width=1, threshold=180)


def hinted_glyph(char, path, size):
    """Fit dense strokes to native pixels without changing face or metrics.

    Supersampled thresholding merges the inner strokes of these glyphs.
    Native monochrome hinting keeps the one-pixel counters open.
    """
    width = glyph(char, path, size).width
    image = Image.new("L", (width, size + 2), 0)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    draw.text((0, size - 1), char, anchor="ls",
              font=ImageFont.truetype(str(path), size), fill=255)
    return image.convert("1")


def return_slogan(font_path, lishu_path):
    # Slightly larger LiSu; strengthen at 4x before reducing to binary pixels.
    text = "精确 稳定 可靠"
    widths = [glyph(char, font_path, 22).width for char in text]
    image = Image.new("1", (sum(widths), 24), 0)
    x = 0
    for char, advance in zip(text, widths):
        part = return_lishu_glyph(char, lishu_path)
        if char != " ":
            image.paste(part, (x + (advance - part.width) // 2, 0))
        x += advance
    return image


def return_artwork(reference, font_path, lishu_path):
    """Replace ordinary text while preserving the brand, icons and rules.

    Bounds below use the full 384x168 reference coordinates.
    All three lower captions use the same 22px LiSu glyphs.
    """
    page = reference.crop((113, 1, 379, 167)).point(
        lambda p: 255 if p >= 128 else 0, "1")
    for bounds, text, size, baseline in (
            ((156, 10, 239, 35), "返回主页", 20, 32),
            ((141, 52, 250, 75), "是否返回主页?", 16, 70),
            ((143, 110, 239, 138), "三为矿安", 22, 134),
            ((249, 110, 347, 138), "安全相伴", 22, 134),
            ((163, 141, 329, 155), "精确 · 稳定 · 可靠", 11, 153)):
        left, top, right, bottom = bounds
        page.paste(0, (left - 113, top - 1, right - 113, bottom - 1))
        images = [return_lishu_glyph(char, lishu_path)
                  if text in ("三为矿安", "安全相伴")
                  else glyph(char, font_path, size) for char in text]
        width = sum(image.width for image in images)
        if width > right - left:
            raise ValueError(f"Return-home text exceeds its region: {text}")
        x = left + (right - left - width) // 2
        for image in images:
            page.paste(image, (x - 113, baseline - size))
            x += image.width
    return page


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyhbd.ttc"))
    parser.add_argument("--lishu-font", type=Path, default=Path("C:/Windows/Fonts/SIMLI.TTF"))
    args = parser.parse_args()
    chars = sorted(set(TEXT.replace("\n", "") + "°·") |
                   {chr(code) for code in range(32, 127)} | source_characters())
    parts = ["/* Generated by tools/generate_menu_assets.py; XBMP, LSB first. */\n",
             "#ifndef UI_MENU_ASSETS_H\n#define UI_MENU_ASSETS_H\n#include <stdint.h>\n",
             "typedef struct { uint16_t code; uint8_t width; const uint8_t *bits; } ui_menu_glyph_t;\n"]
    for size in (11, 14, 16, 18):
        images = [(char, warning_glyph(args.font, size)
                   if char == "警" and size in (14, 16)
                   else hinted_glyph(char, args.font, size)
                   if char == "置" and size in (16, 18)
                   else glyph(char, args.font, size)) for char in chars]
        for char, image in images:
            parts.append(array(f"ui_menu_{size}_{ord(char):04x}", image))
        parts.append(f"static const ui_menu_glyph_t ui_menu_glyphs_{size}[] = {{\n")
        parts.extend(f"    {{0x{ord(char):04x}, {image.width}u, ui_menu_{size}_{ord(char):04x}}},\n"
                     for char, image in images)
        parts.append("};\n")
    # Crop only static symbols. The screenshot labels/readings are not copied.
    nav_reference = Image.open(REFERENCE / REFERENCE_NAMES[-1]).convert("L")
    for index, name in enumerate(REFERENCE_NAMES):
        reference = Image.open(REFERENCE / name).convert("L")
        top = 12 + 19 * index
        nav = nav_reference.crop((14, top, 27, top + 13)).point(
            (lambda p: 255 if p >= 128 else 0) if index == 7 else
            (lambda p: 255 if p < 128 else 0), "1")
        title = reference.crop((118, 10, 147, 37)).point(lambda p: 255 if p >= 128 else 0, "1")
        parts.append(array(f"ui_menu_nav_{index}", nav))
        parts.append(array(f"ui_menu_title_{index}", title))
    for name, index, bounds in (
            ("address", 0, (324, 73, 371, 125)),
            ("other", 5, (326, 68, 372, 126))):
        detail = Image.open(REFERENCE / REFERENCE_NAMES[index]).convert("L").crop(bounds)
        detail = detail.point(lambda p: 255 if p >= 128 else 0, "1")
        parts.append(array(f"ui_menu_detail_{name}", detail))
    # The current sidebar is drawn separately; only brand text keeps its font.
    reference = Image.open(RETURN_REFERENCE).convert("L")
    if reference.size != (1152, 504):
        raise ValueError("Return-home reference must be the supplied 3x 384x168 image")
    reference = reference.resize((384, 168), Image.Resampling.NEAREST)
    return_page = return_artwork(reference, args.font, args.lishu_font)
    parts.append(array("ui_menu_return_page", return_page))
    slogan = return_slogan(args.font, args.lishu_font)
    parts.append(array("ui_menu_return_slogan", slogan))
    parts.append(f"#define UI_RETURN_SLOGAN_WIDTH {slogan.width}u\n")
    parts.append("#define UI_RETURN_SLOGAN_SIZE 22u\n#define UI_RETURN_SLOGAN_HEIGHT 24u\n")
    parts.append("static const uint8_t *const ui_menu_nav_icons[] = {\n    " +
                 ", ".join(f"ui_menu_nav_{i}" for i in range(8)) + "\n};\n")
    parts.append("static const uint8_t *const ui_menu_title_icons[] = {\n    " +
                 ", ".join(f"ui_menu_title_{i}" for i in range(8)) + "\n};\n#endif\n")
    OUTPUT.write_text("".join(parts), encoding="utf-8")
    print(f"Generated {OUTPUT}: {len(chars)} glyphs in each size, 8 icon pairs")
    print("sha256:", hashlib.sha256(OUTPUT.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
