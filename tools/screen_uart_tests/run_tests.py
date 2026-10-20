"""Compile real UART/parser/task functions against deterministic host boundaries."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
APP = ROOT / "CH584_V1_0_1/APP"
OUT = HERE / "out"


def masked_c(source):
    # Retain byte positions; braces in comments/strings must not affect extraction.
    pattern = r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, lambda m: " " * len(m.group()), source)


def function(source, name):
    masked = masked_c(source)
    match = re.search(r"(?:static\s+)?(?:void|uint8_t|uint16_t|uint32_t)\s+"
                      + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", masked)
    if not match:
        raise RuntimeError("Production function missing: " + name)
    opening = masked.index("{", match.start())
    level = 1
    end = opening + 1
    while level:
        level += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return source[match.start():end]


def main():
    OUT.mkdir(exist_ok=True)
    source_bytes = (APP / "Usart3_task.c").read_bytes()
    source = source_bytes.decode("utf-8-sig")
    header_bytes = (APP / "include/yuying_TFT.h").read_bytes()
    header = header_bytes.decode("utf-8-sig")
    data_types = header[header.index("enum MENU_STATE"):header.index("void UI_Control")]
    messages = "\n".join(line for line in header.splitlines()
                         if line.startswith("#define UI_MSG_"))
    ui_bytes = (APP / "yuying_TFT.c").read_bytes()
    ui_source = ui_bytes.decode("utf-8-sig")
    message_logic = ("volatile uint8_t g_ui_msg = UI_MSG_NONE;\n"
                     "static uint8_t s_ui_msg_hold = 0u;\n"
                     + function(ui_source, "ui_show_msg").replace(
                         "void ui_show_msg(", "void host_real_ui_show_msg(", 1)
                     + "\n" + function(ui_source, "ui_msg_tick_sec"))
    state = source[:source.index("__INTERRUPT")]
    state = re.sub(r'^\s*#include[^\n]*', '', state, flags=re.M)
    state += "\n" + "\n".join(line for line in source.splitlines()
                               if line.startswith("#define APP_UART_"))
    production = "\n".join(function(source, name) for name in (
        "calculate_checksum", "app_uart_process", "usart_ProcessEvent", "parse_received_frame"))
    combined = ("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n"
                + data_types + "\n" + messages + "\n" + message_logic + "\n"
                + f'#include "{(APP / "include/screen_power.h").as_posix()}"\n'
                + (HERE / "host_boundaries.h").read_text(encoding="utf-8")
                + "\n" + state + "\n" + production + "\n"
                + (HERE / "test_cases.c").read_text(encoding="utf-8"))
    (OUT / "production_snapshot.c").write_text(combined, encoding="utf-8")
    vcvars = Path(r"D:\visual studio\visual studio2026\VC\Auxiliary\Build\vcvars64.bat")
    build = f'call "{vcvars}" >nul\ncl /nologo /std:c17 /utf-8 /W3 production_snapshot.c /Fe:screen_uart_tests.exe\n'
    (OUT / "build.cmd").write_text(build, encoding="ascii")
    result = subprocess.run(["cmd.exe", "/c", "build.cmd"], cwd=OUT,
                            capture_output=True, text=True, errors="replace")
    (OUT / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.returncode)
    result = subprocess.run([str(OUT / "screen_uart_tests.exe")], cwd=OUT,
                            capture_output=True, text=True, errors="replace")
    (OUT / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    hashes = {str(APP / "Usart3_task.c"): hashlib.sha256(source_bytes).hexdigest(),
              str(APP / "yuying_TFT.c"): hashlib.sha256(ui_bytes).hexdigest(),
              str(APP / "include/yuying_TFT.h"): hashlib.sha256(header_bytes).hexdigest(),
              str(APP / "include/screen_power.h"): hashlib.sha256(
                  (APP / "include/screen_power.h").read_bytes()).hexdigest()}
    (OUT / "result.json").write_text(json.dumps({"exit_code": result.returncode,
                                                "sha256": hashes}, indent=2), encoding="utf-8")
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
