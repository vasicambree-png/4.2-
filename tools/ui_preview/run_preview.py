"""Build offline before/after UI renderers, save PNGs and compare retained pages."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import zlib

from prepare_host import output_directory

HERE = Path(__file__).resolve().parent


def png_chunk(kind: bytes, content: bytes) -> bytes:
    return struct.pack(">I", len(content)) + kind + content + struct.pack(">I", zlib.crc32(kind + content) & 0xffffffff)


def pgm_to_png(path: Path, scale: int = 1, destination: Path | None = None) -> Path:
    magic, dimensions, levels, pixels = path.read_bytes().split(b"\n", 3)
    width, height = map(int, dimensions.split())
    if magic != b"P5" or levels != b"255" or len(pixels) != width * height:
        raise ValueError("Unexpected renderer image: " + str(path))
    rows = []
    for y in range(height):
        original = pixels[y * width:(y + 1) * width]
        row = bytes(value for value in original for _ in range(scale))
        rows.extend([b"\0" + row] * scale)
    content = b"\x89PNG\r\n\x1a\n"
    content += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width * scale, height * scale, 8, 0, 0, 0, 0))
    content += png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
    content += png_chunk(b"IEND", b"")
    destination = destination or path.with_name(path.stem + (f"_{scale}x" if scale > 1 else "") + ".png")
    destination.write_bytes(content)
    return destination


def retained_case(name: str, scope: str = "home") -> bool:
    if scope == "unified":
        return False  # This task intentionally changes text on all pages.
    if scope == "menus":
        return (name.startswith(("home_", "message_", "save_message_tick")) or
                name in {"power_off_message_tick10", "restart_confirmation"})
    return (name.startswith(("menu_", "subpage_", "message_", "networking_")) or
            name in {"save_message_tick1", "power_off_message_tick10", "restart_confirmation"})


def report(phases: list[str], output: Path, scope: str) -> dict:
    results = {}
    case_names = {}
    equivalents = []
    for phase in phases:
        out = output / phase
        summary = json.loads((out / "summary.json").read_text(encoding="utf-8"))
        if summary.get("preview_scope", "home") != scope:
            raise RuntimeError(f"{phase} results were generated for another scope")
        with (out / "cases.tsv").open(encoding="utf-8", newline="") as stream:
            cases = list(csv.DictReader(stream, delimiter="\t"))
        case_names[phase] = {case["case"] for case in cases}
        findings = [case for case in cases if any(int(case.get(key, 0)) for key in
                    ("missing_glyphs", "boundary_events", "data_mutations", "redraw_mismatches",
                     "bitmap_state_changes", "region_violations"))]
        for name in sorted(case_names[phase]):
            path = out / (name + ".pgm")
            pgm_to_png(path)
            if scope in ("menus", "unified"):
                pgm_to_png(path, 3)
        if scope == "home":
            pgm_to_png(out / "home_example.pgm", 4)
        if scope in ("menus", "unified"):
            for prefix in ("menu_names_page_", "menu_signal_page_", "menu_voltage_page_"):
                group = sorted(name for name in case_names[phase] if name.startswith(prefix))
                if len(group) > 1:
                    reference = (out / (prefix + "1.pgm")).read_bytes()
                    equivalents.append({"phase": phase, "group": prefix, "cases": group,
                                        "page_mapping_valid": all(((out / (name + ".pgm")).read_bytes() == reference) ==
                                                                  (phase == "after" or not name.endswith("_2")) for name in group)})
        compile_text = (out / "compile.log").read_text(encoding="utf-8", errors="replace")
        warnings = re.findall(r"\bwarning (C\d+):", compile_text)
        errors = re.findall(r"\berror ((?:C|LNK)\d+):", compile_text)
        results[phase] = {**summary, "case_findings": findings,
                         "host_compiler_warnings": {code: warnings.count(code) for code in sorted(set(warnings))},
                         "host_compiler_errors": {code: errors.count(code) for code in sorted(set(errors))},
                         "source": json.loads((out / "source_manifest.json").read_text(encoding="utf-8"))}
    comparisons = []
    if "before" in phases and "after" in phases:
        for name in sorted(case_names["before"] | case_names["after"]):
            if not retained_case(name, scope):
                continue
            before = output / "before" / (name + ".pgm")
            after = output / "after" / before.name
            before_bytes = before.read_bytes() if name in case_names["before"] else None
            after_bytes = after.read_bytes() if name in case_names["after"] else None
            comparisons.append({"case": name, "pixel_identical": before_bytes is not None and before_bytes == after_bytes,
                                "before_sha256": hashlib.sha256(before_bytes).hexdigest() if before_bytes is not None else None,
                                "after_sha256": hashlib.sha256(after_bytes).hexdigest() if after_bytes is not None else None})
    result = {"phases": results, "retained_page_comparisons": comparisons,
              "retained_pages_pixel_identical": all(case["pixel_identical"] for case in comparisons) if comparisons else None,
              "preview_scope": scope, "output_directory": str(output),
              "equivalent_page_comparisons": equivalents,
              "text_bounds_include_draw_color_zero": True,
              "scope": "Offline UI, real project fonts and drawing algorithms; no peripheral, transport, scheduler or hardware verification."}
    (output / "report.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = ["# UI 离线验证", "", "使用完整当前 UI 翻译单元、工程实际字体/UTF-8 解码/位图及几何绘制算法，外设端点为空操作。", "",
             f"范围：`{scope}`；输出目录：`{output}`。", "",
             "| 阶段 | 用例 | 缺字 | 越界事件 | 数据改写 | 重画像素不一致 | 位图模式未恢复 | 主页分页契约违规 |", "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for phase, data in results.items():
        lines.append(f"| {phase} | {data['cases']} | {data['missing_glyph_occurrences']} | {data['boundary_events']} | {data['data_mutations']} | {data['redraw_mismatches']} | {data.get('bitmap_state_changes', 0)} | {data.get('home_contract_violations', 0)} |")
    lines.append("")
    for phase, data in results.items():
        lines.append(f"宿主编译 {phase}：警告 {data['host_compiler_warnings']}，错误 {data['host_compiler_errors']}；完整记录见该阶段 `compile.log`。")
    retained_label = "主页、消息提示、保存提示时基和重启提示" if scope == "menus" else "菜单、子页、保存和关机提示"
    lines += ["", f"{retained_label}逐像素对比：{len(comparisons)} 个用例，" +
              ("全部相同。" if comparisons and result["retained_pages_pixel_identical"] else "有变化或尚未运行完整对比。"), "",
              "主页用例包括两页正常/空通道/uint16 极值、host_num=0、普通最大发送地址、sub_num 最大值、倾角极值、开关机状态和发送地址 0/121/122。每个用例检查三次绘制：初次、连续刷新、重新初始化离线绘图上下文。", "",
              "主页分页契约检查：before 按实际绘图识别旧版布局；after 两页共用 (9,5) 的88×44 Logo与两翼各两条斜线。第一页基线44为左侧分站号/右侧本机号，两页信息栏左108、右366，各向中间内收6px；右上电压通常16px（文字宽度超过58px时回退14px）、基线29、右锚376，普通电压旁29×21无线图标顶部10，电压字宽超过46px时改用21×17紧凑图标顶部14，图标与电压间隔3px；第二页右上状态14px、基线23、右锚372，下方基线44为绑定数/台与已用通道，无电压、无线图标、报警，台与数字分开4px。两列数据均18px、22px行距、基线70/92/114/136/158。检查真实墨迹、最长地址、等待和实际零值、无线8/9/10/11图标选择（用户确认8斜杠、9弱、10中、11强，其他值回退斜杠），同时检查紧凑图形与7/12等边界；主控字段真实语义不由绘图测试证明。逐用例结果见 `cases.tsv`、`draws.tsv` 和 `text_contract.json`。", "",
              "每次绘制前后比较页面输入、20 通道、绑定表、扫描缓存、绑定数量、告警数量、名称检查计数和当前消息码。所有页面检查绘图前后的位图透明模式。", "",
              "文字边界统计包含 draw_color=0 的反白文字；不因像素最终为白色而跳过。宿主启动时用真实字库验证右侧越界及整体位于屏幕左侧的反白文字都能被检出。", "",
              "输出：原生分辨率 PNG、" + ("每个用例的 `*_3x.png`。" if scope in ("menus", "unified") else "`after/home_example_4x.png` 及两页正常画面的 `home_page1_3x.png` / `home_page2_3x.png`。") + "放大均为整数倍最近邻；另有 `draws.tsv`、`cases.tsv` 和 `report.json`。", "",
              "本工具不验证 UART/BLE/Flash/刷新调度或实物显示；这些需要相应构建、联机或硬件检查。", ""]
    if scope in ("menus", "unified"):
        lines += [("统一字体范围允许全部页面文字变化，不要求旧字体逐像素一致；使用真实渲染、输入不改写、重绘一致性和文字布局检查。" if scope == "unified" else "菜单范围允许二三级菜单视觉变化，要求所有 home_*、message_*、save_message_tick*、power_off_message_tick10 和 restart_confirmation 前后逐像素一致。"), "",
                  "额外菜单用例覆盖所有合法焦点、子页按钮、0/32 个绑定、三条轮显的全部 11 页、29 字节最长名称、0/121/122/65535 上传地址、参数极值、RSSI -128/-1/0/127 和电压 0/0.9/1.0/9.9/10.0/25.5 V、20 通道空/稀疏/完整数据，以及交替页面。", "",
                  "名称、RSSI、电压页：after 一屏显示20通道，左列1–10、右列11–20，11px文字与12px行距；只有返回按钮，无下一页和分页标记。chu_num2=0/1/2/65535 的画面要求一致；before仍核对原有两页映射。检查通道1/10/11/20的独立数值、20个等待或零值、19个零值与1个未收通道，以及进入/旧下一页/退出/重入帧序列。", ""]
        for group in equivalents:
            lines.append(f"- {group['phase']} {group['group']}: {'一致' if group['page_mapping_valid'] else '存在差异'}。")
        lines.append("")
    for comparison in comparisons:
        if not comparison["pixel_identical"]:
            lines.append(f"- 不应变化的页面 `{comparison['case']}` 前后存在差异或结果缺失。")
    for phase, data in results.items():
        if data["case_findings"]:
            lines += [f"## {phase} 发现", ""]
            for finding in data["case_findings"]:
                lines.append(f"- {finding['case']}: 缺字 {finding['missing_glyphs']}，越界 {finding['boundary_events']}，数据改写 {finding['data_mutations']}，重绘差异 {finding['redraw_mismatches']}，位图模式 {finding.get('bitmap_state_changes', 0)}。")
            lines.append("")
    if scope == "home" and "after" in phases:
        after_dir = output / "after"
        for source_name, target_name in (("home_example", "home_page1_3x"),
                                         ("home_page2", "home_page2_3x")):
            source = after_dir / f"{source_name}.pgm"
            if source.exists():
                pgm_to_png(source, 3, after_dir / f"{target_name}.png")
    (output / "report.md").write_text("\n".join(lines), encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("before", "after", "both"), default="both")
    parser.add_argument("--report-only", action="store_true")
    parser.add_argument("--output", help="Output subdirectory beneath this tool; defaults to output or menu_output by scope")
    parser.add_argument("--scope", choices=("home", "menus", "unified"), default="home")
    parser.add_argument("--baseline-sha256", help="Require the frozen session source to have this SHA256")
    args = parser.parse_args()
    output = output_directory(args.output, args.scope)
    phases = ["before", "after"] if args.phase == "both" else [args.phase]
    if not args.report_only:
        for phase in phases:
            prepare = [sys.executable, str(HERE / "prepare_host.py"), phase,
                       "--output", str(output), "--scope", args.scope]
            if args.baseline_sha256:
                prepare += ["--baseline-sha256", args.baseline_sha256]
            subprocess.run(prepare, check=True)
            build_started = time.time()
            completed = subprocess.run(["cmd", "/d", "/c", str(HERE / "build_host.cmd"), phase, str(output), args.scope], cwd=HERE)
            summary_path = output / phase / "summary.json"
            if not summary_path.exists() or summary_path.stat().st_mtime < build_started:
                raise RuntimeError(f"{phase} host build or execution did not produce summary.json (exit {completed.returncode})")
    result = report(phases, output, args.scope)
    for phase, data in result["phases"].items():
        print(f"{phase}: {data['cases']} cases, missing={data['missing_glyph_occurrences']}, boundary={data['boundary_events']}, mutations={data['data_mutations']}, redraw={data['redraw_mismatches']}")
    print(f"Retained pages identical: {result['retained_pages_pixel_identical']}")
    print("Report:", output / "report.md")
    contract_failures = False
    if args.scope in ("home", "unified") and "after" in phases:
        from check_unified_text import check
        contracts = check(output, home_only=args.scope == "home")
        contract_failures = bool(contracts["failures"])
        result["text_layout_contracts"] = contracts
        (output / "report.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        with (output / "report.md").open("a", encoding="utf-8") as stream:
            stream.write(f"\n实际文字墨迹及布局检查：{contracts['checks']} 项，失败 {len(contracts['failures'])} 项。\n")
    if args.scope in ("menus", "unified") and "after" in phases:
        from check_param_pages import check
        params = check(output)
        contract_failures = contract_failures or bool(params["failures"])
        result["param_page_contracts"] = params
        (output / "report.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    after = result["phases"].get("after", {})
    return int(contract_failures or any(after.get(key, 0) for key in ("missing_glyph_occurrences", "boundary_events", "data_mutations", "redraw_mismatches", "bitmap_state_changes", "home_contract_violations", "white_text_boundary_probe_failures")) or
               any(not group["page_mapping_valid"] for group in result["equivalent_page_comparisons"] if group["phase"] == "after") or
               result["retained_pages_pixel_identical"] is False)


if __name__ == "__main__":
    raise SystemExit(main())
