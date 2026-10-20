"""Prepare a host-only copy of the complete UI translation unit.

The source, headers, font decoder, UTF-8 decoder, drawing algorithms and font
bytes come from the active firmware. No firmware or historical output is edited.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
FIRMWARE = ROOT / "CH584_V1_0_1"


def output_directory(value: str | None, scope: str) -> Path:
    """Keep generated previews beside the tool, with separate task snapshots."""
    path = Path(value or ("menu_output" if scope == "menus" else "output"))
    output = (HERE / path).resolve() if not path.is_absolute() else path.resolve()
    try:
        output.relative_to(HERE)
    except ValueError as exc:
        raise ValueError("Preview output must stay inside tools/ui_preview") from exc
    if output == HERE:
        raise ValueError("Preview output must be a dedicated subdirectory")
    return output


CONFIG = r"""#ifndef UI_PREVIEW_CONFIG_H
#define UI_PREVIEW_CONFIG_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __HIGH_CODE
#define GPIO_Pin_0 1
#define GPIO_Pin_1 2
#define GPIO_Pin_2 4
#define GPIO_Pin_3 8
#define GPIO_Pin_12 4096
#define GPIO_Pin_13 8192
#define GPIO_Pin_14 16384
#define GPIO_ModeOut_PP_5mA 0
#define GPIOA_SetBits(...) ((void)0)
#define GPIOA_ResetBits(...) ((void)0)
#define GPIOB_SetBits(...) ((void)0)
#define GPIOB_ResetBits(...) ((void)0)
#define GPIOA_ModeCfg(...) ((void)0)
#define GPIOB_ModeCfg(...) ((void)0)
#define SPI0_MasterDefInit(...) ((void)0)
#define SPI0_MasterDMATrans(...) ((void)0)
#define DelayMs(...) ((void)0)
#define DelayUs(...) ((void)0)
#endif
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("before", "after"), nargs="?", default="after")
    parser.add_argument("--output", help="Output subdirectory beneath this tool")
    parser.add_argument("--scope", choices=("home", "menus", "unified"), default="home")
    parser.add_argument("--baseline-sha256", help="Require the frozen session source to have this SHA256")
    args = parser.parse_args()
    output = output_directory(args.output, args.scope)
    output.mkdir(parents=True, exist_ok=True)
    snapshot = output / "yuying_TFT.session_before.c"
    if not snapshot.exists():
        if args.baseline_sha256:
            raise RuntimeError("Expected frozen session snapshot is missing: " + str(snapshot))
        snapshot.write_bytes((FIRMWARE / "APP/yuying_TFT.c").read_bytes())
    snapshot_sha256 = hashlib.sha256(snapshot.read_bytes()).hexdigest()
    if args.baseline_sha256 and snapshot_sha256 != args.baseline_sha256.lower():
        raise RuntimeError("Frozen session snapshot SHA256 does not match the required baseline")
    source = snapshot if args.phase == "before" else FIRMWARE / "APP/yuying_TFT.c"
    out = output / args.phase
    out.mkdir(exist_ok=True)
    raw_source = source.read_bytes()
    text = raw_source.decode("utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
    # MSVC rejects this unused GNU zero-element array; change only the host copy.
    text, count = re.subn(r"const uint8_t bmp\[\]\s*=\s*\{\s*\};",
                          "const uint8_t bmp[1]={0};", text, count=1)
    if count != 1:
        raise RuntimeError("Expected unused zero-element bmp declaration was not found")
    (out / "ui_under_test.c").write_text(text, encoding="utf-8")
    (out / "CONFIG.h").write_text(CONFIG, encoding="utf-8")
    asset_hashes = {}
    for asset_name in ("ui_home_assets.h", "ui_menu_assets.h", "ui_wireless_assets.h"):
        if f'#include "{asset_name}"' not in text:
            continue
        assets = FIRMWARE / "APP/include" / asset_name
        asset_snapshot = output / (Path(asset_name).stem + ".session_before.h")
        if args.phase == "before" and not asset_snapshot.exists():
            asset_snapshot.write_bytes(assets.read_bytes())
        asset_source = asset_snapshot if args.phase == "before" else assets
        asset_bytes = asset_source.read_bytes()
        (out / asset_name).write_bytes(asset_bytes)
        asset_hashes[asset_name] = hashlib.sha256(asset_bytes).hexdigest()

    active_text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    fonts = {"u8g2_font16_lunar", "u8g2_font24_lunar", "u8g2_font_5x8_tr",
             "u8g2_font_helvB10_tr", "u8g2_font_helvB12_tr"}
    fonts.update(re.findall(r"\bu8g2_font[A-Za-z0-9_]+\b", active_text))
    fonts.discard("u8g2_font_wqy14_t_gb2312a")  # Real application font .c is compiled directly.
    font_source = (FIRMWARE / "u8g2/u8g2_fonts.c").read_text(encoding="utf-8")
    parts = ['#include "u8g2.h"\n']
    for name in sorted(fonts):
        match = re.search(r"const uint8_t " + re.escape(name) +
                          r"\[.*?\]\s+U8G2_FONT_SECTION\(.*?\)\s*=\s*(?:\"(?:\\.|[^\"\\])*\"\s*)+;",
                          font_source, re.S)
        if not match:
            raise RuntimeError("Actual project font definition not found: " + name)
        parts.append(match.group(0) + "\n")
    (out / "local_fonts.c").write_text("".join(parts), encoding="utf-8")
    for filename, substitutions in {
        "font_engine.c": {"u8g2_DrawUTF8": "real_u8g2_DrawUTF8"},
        "bitmap_engine.c": {"u8g2_DrawXBM": "real_u8g2_DrawXBM", "u8g2_DrawXBMP": "real_u8g2_DrawXBMP"},
        "box_engine.c": {name: "real_" + name for name in ("u8g2_DrawFrame", "u8g2_DrawRFrame", "u8g2_DrawBox", "u8g2_DrawRBox")},
    }.items():
        original = {"font_engine.c": "u8g2_font.c", "bitmap_engine.c": "u8g2_bitmap.c", "box_engine.c": "u8g2_box.c"}[filename]
        wrapper = "".join(f"#define {name} {renamed}\n" for name, renamed in substitutions.items())
        wrapper += f'#include "{(FIRMWARE / "u8g2" / original).as_posix()}"\n'
        (out / filename).write_text(wrapper, encoding="utf-8")
    metadata = {
        "phase": args.phase, "source": str(source),
        "preview_scope": args.scope,
        "source_sha256": hashlib.sha256(raw_source).hexdigest(),
        "session_before_sha256": snapshot_sha256,
        "ui_home_assets_sha256": asset_hashes.get("ui_home_assets.h"),
        "ui_menu_assets_sha256": asset_hashes.get("ui_menu_assets.h"),
        "fonts": sorted(fonts) + ["u8g2_font_wqy14_t_gb2312a"],
        "host_only_transform": "unused zero-element bmp array changed to one zero byte",
        "real_algorithms": ["u8g2_font.c", "u8x8_8x8.c", "u8g2_bitmap.c", "u8g2_box.c", "u8g2_line.c", "u8g2_circle.c"],
    }
    (out / "source_manifest.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Prepared {args.phase}: {metadata['source_sha256']}")


if __name__ == "__main__":
    main()
