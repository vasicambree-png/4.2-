"""Check received/waiting data and internal layout using actual renderer logs."""
from collections import defaultdict
import json
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def check(output, home_only=False):
    header = (ROOT / "CH584_V1_0_1/APP/include/ui_menu_assets.h").read_text(encoding="utf-8")
    tables = {}
    for size in (11, 14, 16, 18):
        body = re.search(rf"ui_menu_glyphs_{size}\[\] = \{{(.*?)\}};", header, re.S).group(1)
        tables[size] = {int(code, 16): int(width) for code, width in
                        re.findall(r"\{0x([0-9a-f]+), (\d+)u,", body)}
    records = defaultdict(list)
    inks = defaultdict(list)
    logos = defaultdict(list)
    bitmaps = defaultdict(list)
    top_layouts = {}
    pages = {parts[0]: int(parts[5]) for line in (output / "after/cases.tsv").read_text(encoding="utf-8").splitlines()[1:]
             if (parts := line.split("\t"))}
    for line in (output / "after/draws.tsv").read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if parts[0] == "CUSTOM_TEXT" and parts[2] == "primary":
            x, y, size, missing = map(int, parts[3:7])
            records[parts[1]].append((x, y, size, parts[7]))
            assert not missing, line
        elif parts[0] == "CUSTOM_INK" and parts[2] == "primary":
            inks[parts[1]].append(tuple(map(int, parts[3:10])) + (parts[10],))
        elif parts[0] == "XBMP" and parts[2] == "primary":
            x, y, w, h = map(int, parts[3:7])
            bitmaps[parts[1]].append((x, y, w, h))
            if w >= 60 and h >= 30 and x == 9 and y == 5:
                logos[parts[1]].append((x, y, w, h))
        elif parts[0] == "HOME_TOP_LAYOUT":
            top_layouts[parts[1]] = tuple(map(int, parts[2:7]))
    failures, checks = [], []

    def require(condition, description):
        checks.append(description)
        if not condition:
            failures.append(description)

    def width(record):
        return sum(tables[record[2]][ord(char)] for char in record[3])

    def count(case, text):
        return sum(row[3] == text for row in records[case])

    for case, rows in records.items():
        if case.startswith("home_") or case == "save_message_tick2_home":
            require(logos[case] == [(9, 5, 88, 44)], case + ": shared enlarged logo position and size")
            states = [row for row in rows if row[3].startswith("状态")]
            is_page2 = pages[case] == 2
            voltage = [row for row in rows if re.fullmatch(r"\d+\.\d{2}V", row[3])]
            wifi = [bitmap for bitmap in bitmaps[case] if bitmap[2:] in ((29, 21), (21, 17))]
            require(not any(row[3].startswith("报警:") for row in rows), case + ": alarm removed from home metadata")
            if not is_page2:
                require(not states and len(voltage) == 1 and len(wifi) == 1,
                        case + ": page one shows voltage and wireless bitmap without status")
                for value in voltage:
                    size = 16 if sum(tables[16][ord(char)] for char in value[3]) <= 58 else 14
                    require(value[1:3] == (29, size) and value[0] + width(value) == 376,
                            case + ": page-one voltage at the top-right baseline")
                    large = width(value) <= 46
                    icon_w, icon_h, icon_y = (29, 21, 10) if large else (21, 17, 14)
                    require(len(wifi) == 1 and wifi[0] == (value[0] - icon_w - 3, icon_y, icon_w, icon_h) and
                            wifi[0][0] >= (298 if large else 293),
                            case + ": proportioned wireless icon sits three pixels left of the voltage and clears the title wing")
                machine = next(row for row in rows if row[3].startswith("本机"))
                station = next(row for row in rows if row[3].startswith("分站"))
                require(machine[1:3] == (44, 14) and machine[0] + width(machine) == 366 and
                        station[:3] == (108, 44, 14) and station[3].startswith("分站号:"),
                        case + ": full address labels use the shared 14px metadata font")
                require(machine[0] - station[0] - width(station) >= 4,
                        case + ": longest station and machine groups retain a clear gap")
            else:
                require(len(states) == 1 and not voltage and not wifi,
                        case + ": page two shows status without voltage or wireless bitmap")
                for state in states:
                    require(state[1:3] == (23, 14) and state[0] + width(state) == 372 and
                            state[3] in ("状态:开机", "状态:关机"), case + ": complete status at top right")
            stats = [row for row in rows if row[3].startswith(("已绑定:", "已用通道:"))]
            units = [row for row in rows if row[1] == 44 and row[3] == "台"]
            require((not is_page2 and not stats and not units) or
                    (is_page2 and len(stats) == 2 and len(units) == 1),
                    case + ": exactly one page metadata variant")
            if is_page2 and len(stats) == 2 and len(units) == 1:
                bound = next(row for row in stats if row[3].startswith("已绑定:"))
                used = next(row for row in stats if row[3].startswith("已用通道:"))
                unit = units[0]
                require(bound[:3] == (108, 44, 14) and used[1:3] == (44, 14) and
                        used[0] + width(used) == 366 and unit[1:3] == (44, 14),
                        case + ": binding and used channels retain the shared metadata baseline")
                require(unit[0] - bound[0] - width(bound) == 4 and
                        used[0] - unit[0] - width(unit) >= 4,
                        case + ": binding count has a separate unit with a four-pixel advance gap")
            if case in top_layouts:
                page, wifi_count, wifi_hash, wifi_ink, wing_failures = top_layouts[case]
                require(page == (2 if is_page2 else 1) and not wing_failures,
                        case + ": both mirrored title wings have exactly two black stripes")
                require(wifi_count == (0 if is_page2 else 1) and (wifi_ink == 0 if is_page2 else wifi_ink > 0),
                        case + ": wireless bitmap region follows the selected page")
            elif case != "save_message_tick2_home" and not case.startswith("home_after_menus_"):
                require(False, case + ": host records the top-layout pixel contract")
            data_rows = [row for row in rows if row[1] >= 52]
            for row in data_rows:
                is_badge = row[3].isdigit() and (row[0] < 40 or 203 <= row[0] < 225)
                require(row[1] in (70, 92, 114, 136, 158) and row[2] == (14 if is_badge else 18),
                        case + ": data baseline/font " + str(row))
            for baseline in (70, 92, 114, 136, 158):
                for left in (14, 203):
                    fields = [row for row in data_rows if row[1] == baseline and left <= row[0] < left + 166]
                    require(len(fields) >= 3 and any(row[0] == left + 24 for row in fields),
                            case + f": complete data row at {left},{baseline}")
            case_inks = [row for row in inks[case] if row[3] >= 0]
            require(len(case_inks) == len(rows), case + ": actual ink available for every drawn text")
            for row in case_inks:
                x, baseline, size, x0, y0, x1, y1, text = row
                if baseline >= 52:
                    row_index = (baseline - 70) // 22
                    left, right = (8, 186) if x < 190 else (197, 375)
                    require(left <= x0 <= x1 <= right and 53 + 22 * row_index <= y0 <= y1 <= 73 + 22 * row_index,
                            case + ": ink inside its data cell " + text)
                elif text.startswith("状态"):
                    require(312 <= x0 <= x1 <= 372 and 10 <= y0 <= y1 <= 26,
                            case + ": status ink inside the top-right header")
                elif text.startswith(("本机", "分站", "已绑定:", "已用通道:")) or text == "台":
                    require(98 <= x0 <= x1 <= 372 and 31 <= y0 <= y1 <= 46,
                            case + ": header ink beside logo " + text)
            for index, a in enumerate(case_inks):
                for b in case_inks[index + 1:]:
                    require(a[5] < b[3] or b[5] < a[3] or a[6] < b[4] or b[6] < a[4],
                            case + ": separate ink " + a[7] + " / " + b[7])
    for case, labels in (
        ("home_state_off_station", ("分站号:64", "本机号:118-->分站")),
        ("home_uint16_max", ("分站号:65535", "本机号:65535-->65535")),
    ):
        require(all(count(case, label) == 1 for label in labels),
                case + ": complete addresses at the shared header size")
    for state in ("on", "off"):
        for bound in (0, 20, 32):
            case = f"home_page2_state_{state}_bound_{bound}"
            require(count(case, "状态:" + ("开机" if state == "on" else "关机")) == 1 and
                    count(case, f"已绑定:{bound}") == 1 and count(case, "台") == 1,
                    case + ": status and binding-count limits displayed together")
    wifi_values = (0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 255)
    wifi_hashes = {value: top_layouts.get(f"home_wifi_{value}", (None,) * 5)[2] for value in wifi_values}
    require(all(value is not None for value in wifi_hashes.values()) and
            len({wifi_hashes[value] for value in (8, 9, 10, 11)}) == 4 and
            all(wifi_hashes[value] == wifi_hashes[8] for value in (0, 1, 2, 3, 4, 7, 12, 255)),
            "HEX codes 8..11 stay distinct and unsupported codes share the crossed icon")
    for prefix in ("home_wifi_", "home_wifi_compact_"):
        layouts = {value: top_layouts.get(f"{prefix}{value}", (None,) * 5) for value in range(8, 13)}
        require(all(layout[2] is not None for layout in layouts.values()) and
                len({layouts[value][2] for value in range(8, 12)}) == 4 and
                layouts[8][2] == layouts[12][2] and
                layouts[9][3] < layouts[10][3] < layouts[11][3],
                prefix + "8 crossed, 9/10/11 add signal arcs, 12 falls back")
    for case in ("home_empty", "home_page2_empty"):
        require(count(case, "通道") == 0 and count(case, "--") == 20,
                case + ": unknown channels use placeholders before binding")
    for case in ("home_bound_waiting", "home_page2_bound_waiting"):
        require(count(case, "--") == 10 and count(case, "0.0") == 0 and count(case, "0") == 0,
                case + ": bound channels await actual data")
    for case in ("home_received_zero", "home_page2_received_zero"):
        require(count(case, "--") == 0 and count(case, "0.0") == 5 and count(case, "0") == 5,
                case + ": actual zero readings are visible")
    require(count("home_partial_received", "--") == 1,
            "home_partial_received: only unread channel waits")
    for kind, zero in (() if home_only else (("signal", "0 dBm"), ("voltage", "0.0 V"))):
        require(count(f"menu_{kind}_waiting", "--") == 20,
                kind + ": twenty unread values wait")
        require(count(f"menu_{kind}_received_zero", zero) == 20,
                kind + ": twenty actual zeros visible")
        require(count(f"menu_{kind}_partial_received", zero) == 18 and
                count(f"menu_{kind}_partial_received", "--") == 2,
                kind + ": partial receipt remains independent")
    if not home_only:
        require(sum(row[3].startswith("SW_") for row in records["menu_names_waiting"]) == 20,
                "binding names available before telemetry")
        lower_names = [row for case in ("menu_names_descenders", "menu_names_descenders_page2")
                       for row in records[case] if "g_jpqy" in row[3]]
        require(len(lower_names) == 4 and all(row[1] == 159 and row[2] == 11 for row in lower_names),
                "last-row descenders leave space above bottom frame")
    result = {"checks": len(checks), "failures": failures}
    (output / "text_contract.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                                               encoding="utf-8")
    print(f"Text/layout contracts: {len(checks)} checks, {len(failures)} failures")
    return result


if __name__ == "__main__":
    sys.exit(bool(check(HERE / "unified_text_output")["failures"]))
