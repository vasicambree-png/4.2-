"""Check all twenty channels and cell ink from actual firmware drawing logs."""
from collections import defaultdict
import csv
import json
from pathlib import Path
import re


def check(output):
    root = Path(__file__).resolve().parents[2]
    header = (root / "CH584_V1_0_1/APP/include/ui_menu_assets.h").read_text(encoding="utf-8")
    slogan_width = int(re.search(r"#define UI_RETURN_SLOGAN_WIDTH (\d+)u", header).group(1))
    slogan_size = int(re.search(r"#define UI_RETURN_SLOGAN_SIZE (\d+)u", header).group(1))
    font_widths = {}
    for size in (11, 18):
        glyphs = re.search(rf"ui_menu_glyphs_{size}\[\] = \{{(.*?)\}};", header, re.S).group(1)
        font_widths[size] = {int(code, 16): int(glyph_width) for code, glyph_width in
                             re.findall(r"\{0x([0-9a-f]+), (\d+)u,", glyphs)}

    def width(text, size=11):
        glyph_widths = font_widths[size]
        return sum(glyph_widths.get(ord(char), glyph_widths[ord('?')]) for char in text)

    records, inks = defaultdict(list), defaultdict(list)
    for line in (output / "after/draws.tsv").read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if parts[0] == "CUSTOM_TEXT" and parts[2] == "primary":
            records[parts[1]].append(tuple(map(int, parts[3:6])) + (parts[7],))
        elif parts[0] == "CUSTOM_INK" and parts[2] == "primary":
            inks[parts[1]].append(tuple(map(int, parts[3:10])) + (parts[10],))
    with (output / "after/cases.tsv").open(encoding="utf-8", newline="") as stream:
        cases = list(csv.DictReader(stream, delimiter="\t"))
    failures, checks = [], []

    def require(condition, message):
        checks.append(message)
        if not condition:
            failures.append(message)

    case_names = {case["case"] for case in cases}
    for count in (0, 1, 7, 10, 11, 20, 32):
        for timer in (0, 4, 8, 12, 65535):
            for button in (0, 1):
                fixture_name = f"subpage_unbind_count_{count}_time_{timer}_button_{button}"
                require(fixture_name in case_names,
                        fixture_name + ": unbind count/timer/action fixture present")
    for mode in range(3):
        for cycle in range(2):
            for button in range(2):
                fixture_name = f"subpage_unbind_mac_{mode}_cycle_{cycle}_button_{button}"
                require(fixture_name in case_names,
                        fixture_name + ": longest-name and extreme full-MAC fixture present")
    for count in (0, 1, 6, 7, 10, 11, 20, 32):
        for cache in (0, 1):
            for button in (0, 1):
                for cycle in range(max(1, (count + 9) // 10) + 1):
                    fixture_name = f"subpage_scan_bound_{count}_cache_{cache}_cycle_{cycle}_button_{button}"
                    require(fixture_name in case_names,
                            fixture_name + ": all-binding count/cache/timer/action fixture present")
    for label in ("signal", "voltage", "names"):
        require(f"menu_{label}_endpoints" in case_names,
                label + ": independent column-endpoint fixture present")
        require(f"menu_20_{label}_waiting" in case_names,
                label + ": twenty-channel waiting fixture present")
    for label in ("signal", "voltage"):
        for mode in ("received_zero", "partial_received"):
            require(f"menu_20_{label}_{mode}" in case_names,
                    label + ": twenty-channel " + mode + " fixture present")

    for case in cases:
        kind = int(case["subpage"])
        menu = int(case["menu"])
        if int(case["rank"]) != 3 or not (menu == 3 and kind in (3, 4) or menu == 6 and kind == 5):
            continue
        name = case["case"]
        rows = records[name]
        data = [row for row in rows if 22 <= row[1] < 164]
        numbers = [row for row in data if row[3].isdigit() and row[2] == 11 and
                   (13 <= row[0] < 33 or 202 <= row[0] < 222)]
        require([int(row[3]) for row in numbers] == list(range(1, 21)),
                name + ": all twenty ordered channel numbers")
        for i, row in enumerate(numbers):
            require(row[1] == 33 + 14 * (i % 10) and
                    ((13 <= row[0] < 33) if i < 10 else (202 <= row[0] < 222)),
                    name + ": ten rows per column " + row[3])
            left = 13 if i < 10 else 202
            require(row[0] == left + (16 - width(row[3])) // 2,
                    name + ": number centered in its gutter " + row[3])
        label = {3: "信号:", 4: "电压:", 5: "名称:"}[int(case["subpage"])]
        require(sum(row[3] == label for row in data) == 0,
                name + ": no repeated row label")
        require(len(data) == 40 and all(row[1] in range(33, 160, 14) and row[2] == 11 for row in data),
                name + ": forty data fields at eleven pixels and fourteen-pixel pitch")
        title = "信息汇总" if kind == 5 else "安装调试"
        subtitle = {3: "信号", 4: "电压", 5: "名称"}[kind]
        require(len(rows) == 42 and (29, 20, 18, title) in rows and
                not any(row[3] == subtitle for row in rows),
                name + ": full-size menu title retained without data subtitle")
        require(sum(row[3] == "返回" and row[0] >= 298 and row[1:3] == (16, 14)
                    for row in rows) == 1,
                name + ": compact return at upper right")
        for row in data:
            if row in numbers:
                continue
            left = 13 if row[0] < 190 else 202
            require(row[0] == left + 24 + (144 - width(row[3])) // 2,
                    name + ": data centered in its slot " + row[3])
        require(sum(row[3] == "返回" for row in rows) == 1 and
                sum(row[3] == "下一页" for row in rows) == 0,
                name + ": one return action and no next-page action")
        require(all("1/2" not in row[3] and "2/2" not in row[3] for row in rows),
                name + ": no page indicator")
        data_ink = [row for row in inks[name] if 22 <= row[1] < 164 and row[3] >= 0]
        require(len(data_ink) == len(data), name + ": visible ink for every data field")
        header_ink = [row for row in inks[name] if row[1] < 22 and row[3] >= 0]
        require(len(header_ink) == 2 and all(7 <= row[3] <= row[5] <= 376 and
                2 <= row[4] <= row[6] <= 22 for row in header_ink),
                name + ": header ink fits above the data grid")
        for i, a in enumerate(header_ink):
            for b in header_ink[i + 1:]:
                require(a[5] < b[3] or b[5] < a[3],
                        name + ": header title and return remain separate")
        for row in data_ink:
            x, baseline, size, x0, y0, x1, y1, text = row
            left, right = (7, 187) if x < 190 else (196, 376)
            top = 23 + 14 * ((baseline - 33) // 14)
            require(left <= x0 <= x1 <= right and top <= y0 <= y1 <= top + 12,
                    name + ": ink inside cell " + text)
        for i, a in enumerate(data_ink):
            for b in data_ink[i + 1:]:
                require(a[5] < b[3] or b[5] < a[3] or a[6] < b[4] or b[6] < a[4],
                        name + ": separate ink " + a[7] + " / " + b[7])
        if "waiting" in name:
            values = [row[3] for row in data if not row[3].isdigit()]
            require(sum(text.startswith("SW_") for text in values) == 20 if int(case["subpage"]) == 5
                    else values.count("--") == 20, name + ": binding names or unread placeholders")
        if "received_zero" in name:
            zero = "0 dBm" if int(case["subpage"]) == 3 else "0.0 V"
            require(sum(row[3] == zero for row in data) == 20 and
                    sum(row[3] == "--" for row in data) == 0,
                    name + ": received zero is distinct from waiting")
        elif "partial_received" in name:
            zero = "0 dBm" if int(case["subpage"]) == 3 else "0.0 V"
            unread = 1 if name.startswith("menu_20_") else 2
            require(sum(row[3] == zero for row in data) == 20 - unread and
                    sum(row[3] == "--" for row in data) == unread,
                    name + ": unread channel remains independent")
        if name.endswith("_endpoints"):
            expected = {3: {1: "-11 dBm", 10: "-42 dBm", 11: "-73 dBm", 20: "-104 dBm"},
                        4: {1: "1.1 V", 10: "4.2 V", 11: "7.3 V", 20: "10.4 V"},
                        5: {1: "SW_CH01", 10: "SW_CH10", 11: "SW_CH11", 20: "SW_CH20"}}[int(case["subpage"])]
            for channel, text in expected.items():
                baseline = 33 + 14 * ((channel - 1) % 10)
                left = 13 if channel <= 10 else 202
                require(any(row[3] == text and row[1] == baseline and
                            left + 24 <= row[0] < left + 168 for row in data),
                        name + ": channel " + str(channel) + " value in its own row")
    for label in ("signal", "voltage", "names"):
        images = [(output / "after" / f"menu_{label}_page_{page}.pgm").read_bytes()
                  for page in (0, 1, 2, 65535)]
        require(all(image == images[0] for image in images),
                label + ": legacy page codes cannot change twenty-channel content")
    for kind in (3, 4, 5):
        prefix = f"pagination_{kind}_"
        images = [(output / "after" / (prefix + step + ".pgm")).read_bytes()
                  for step in ("enter", "next", "exit", "reenter")]
        require(images[0] == images[1] == images[3] and images[2] != images[0],
                prefix + "legacy next frame preserves all channels, exit and reenter are clean")
        left = (output / "after" / f"route_return_left_stale_{kind}.pgm").read_bytes()
        require(left == (output / "after/menu_7.pgm").read_bytes(),
                f"return menu: left selection ignores stale subpage {kind}")
        for menu in range(8):
            if menu == 3 and kind in (3, 4) or menu == 6 and kind == 5:
                continue
            actual = output / "after" / f"route_menu_{menu}_stale_{kind}.pgm"
            require(actual.read_bytes() == (output / "after" / f"menu_all_{menu}_focus_0.pgm").read_bytes(),
                    f"menu {menu}: stale subpage {kind} cannot steal rendering")
    for case in cases:
        if int(case["rank"]) == 3 and not case["case"].startswith("message_"):
            full_data = int(case["menu"]) == 3 and int(case["subpage"]) in (3, 4) or \
                        int(case["menu"]) == 6 and int(case["subpage"]) == 5 or \
                        int(case["menu"]) == 2 and int(case["subpage"]) in (1, 2)
            if not full_data:
                titles = ("地址分区", "组网测试", "设备绑定", "安装调试", "上传设置", "其他设置", "信息汇总", "返回主页")
                require((156, 31, 18, titles[int(case["menu"])]) in records[case["case"]],
                        case["case"] + ": full-size third-level detail title restored")
    for menu in range(8):
        if menu == 2:
            continue
        require((output / "after" / f"route_menu_{menu}_stale_1.pgm").read_bytes() ==
                (output / "after" / f"menu_all_{menu}_focus_0.pgm").read_bytes(),
                f"menu {menu}: unbind flag cannot steal another menu")
    require((output / "after/route_return_left_stale_1.pgm").read_bytes() ==
            (output / "after/menu_7.pgm").read_bytes(),
            "return menu: left selection ignores unbind flag")
    require((output / "after/binding_left_stale_1.pgm").read_bytes() ==
            (output / "after/menu_2.pgm").read_bytes(),
            "binding menu: second-level selection ignores stale unbind flag")
    require((output / "after/binding_save_parent_stale.pgm").read_bytes() ==
            (output / "after/menu_2.pgm").read_bytes(),
            "binding menu: save parent ignores stale binding flag")
    for case in cases:
        name = case["case"]
        rows = records[name]
        if int(case["menu"]) == 7 and int(case["rank"]) in (2, 3):
            versions = [row for row in rows if row[3].startswith("V")]
            require(len(versions) == 1 and re.fullmatch(r"V1_\d+_\d", versions[0][3]) and
                    versions[0][1:3] == (25, 18) and
                    versions[0][0] + width(versions[0][3], 18) == 368,
                    name + ": dynamic version with V1_ prefix at upper-right corner")
            version_inks = [row for row in inks[name] if re.fullmatch(r"V1_\d+_\d", row[7])]
            require(len(version_inks) == 1 and 250 <= version_inks[0][3] <= version_inks[0][5] <= 372 and
                    1 <= version_inks[0][4] <= version_inks[0][6] <= 34,
                    name + ": version ink inside upper-right band")
            if name.startswith("return_version_"):
                raw = int(name.removeprefix("return_version_"))
                require(versions and versions[0][3] == f"V1_{raw // 10}_{raw % 10}",
                        name + ": final segments follow controller tenths encoding")
            slogans = [row for row in inks[name] if row[7] == "精确 稳定 可靠"]
            slogan_rows = [row for row in rows if row[3] == "精确 稳定 可靠"]
            require(len(slogan_rows) == 1 and slogan_rows[0][:3] ==
                    (113 + (266 - slogan_width) // 2, 155, slogan_size),
                    name + ": enlarged slogan centered in footer")
            require(len(slogans) == 1 and len(version_inks) == 1 and
                    version_inks[0][6] < slogan_rows[0][1] + 1 - slogan_size,
                    name + ": slogan and version remain separate")
        if int(case["rank"]) != 3 or int(case["menu"]) != 2 or int(case["subpage"]) not in (1, 2):
            continue
        if int(case["subpage"]) == 2:
            fixture = re.fullmatch(r"subpage_scan_bound_(\d+)_cache_(\d+)_cycle_(\d+)_button_(\d+)", name)
            longest = re.fullmatch(r"subpage_scan_longest_cycle_(\d+)_button_(\d+)", name)
            expected_count = int(fixture[1]) if fixture else 32 if longest else 0 if "empty" in name else 3
            expected_numbers = list(range(1, expected_count + 1))
            column_rows = 10 if expected_count <= 20 else 8
            column_step = 189 if expected_count <= 20 else 92
            row_step = 11 if expected_count <= 20 else 13
            name_width = 147 if expected_count <= 20 else 64
            require((29, 20, 18, "设备绑定") in rows and
                    (108, 20, 18, f"已绑定设备:{expected_count}台") in rows and
                    sum(row[3] == "保存目前设备" for row in rows) == 1 and
                    sum(row[3] == "返回" for row in rows) == 1 and
                    not any(row[3] == "地址分区" for row in rows),
                    name + ": fullscreen scan page retains title, binding count and actions")
            page_rows = [row for row in rows if re.fullmatch(r"\d+/\d+", row[3])]
            require(not page_rows, name + ": every binding is shown without pagination")
            if fixture:
                reference = f"subpage_scan_bound_{expected_count}_cache_0_cycle_0_button_{fixture[4]}"
                require((output / f"after/{name}.pgm").read_bytes() ==
                        (output / f"after/{reference}.pgm").read_bytes(),
                        name + ": time and independent scan cache do not change binding page")
            if longest:
                reference = f"subpage_scan_longest_cycle_0_button_{longest[2]}"
                require((output / f"after/{name}.pgm").read_bytes() ==
                        (output / f"after/{reference}.pgm").read_bytes(),
                        name + ": long-name binding page is independent of timer")
            data = [row for row in rows if 22 <= row[1] < 140]
            labels = [row for row in data if re.fullmatch(r"[1-9]\d*:", row[3])]
            names = [row for row in data if row not in labels]
            require(data == [(14, 33, 11, "--")] if expected_count == 0 else
                    len(labels) == len(names) == len(expected_numbers) and
                    [row[3] for row in labels] == [f"{i}:" for i in expected_numbers],
                    name + ": ordered devices have number prefixes or empty placeholder")
            for i, (label, entry) in enumerate(zip(labels, names)):
                x, y = 14 + column_step * (i // column_rows), 33 + row_step * (i % column_rows)
                require(label[:3] == (x, y, 11) and entry[:3] == (x + 24, y, 11) and
                        width(label[3]) < 24 and width(entry[3]) <= name_width,
                        name + ": all numbered names fit their fixed column cells")
            if longest:
                require(len(names) == len(expected_numbers) and all(row[3].startswith("W") and
                        set(row[3]) <= {"W", "."} and width(row[3]) <= name_width for row in names),
                        name + ": longest names fit their column using existing clipping")
            elif expected_count:
                def clipped(text):
                    while width(text) > name_width:
                        text = text[:-1]
                    return text
                require([row[3] for row in names] == [clipped(f"SW_{i:02d}_WY_01-04") for i in expected_numbers],
                        name + ": binding-list order is preserved independently of scan cache")
            scan_ink = [row for row in inks[name] if 22 <= row[1] < 140 and row[3] >= 0]
            require(all(14 <= row[3] <= row[5] < 378 and 22 <= row[4] <= row[6] < 140
                    for row in scan_ink), name + ": names fit above action buttons")
            for row in scan_ink:
                left = 14 + column_step * ((row[0] - 14) // column_step)
                right = left + 24 + name_width
                require(left <= row[3] <= row[5] <= right,
                        name + ": number and name ink stay inside their column")
            for i, a in enumerate(scan_ink):
                for b in scan_ink[i + 1:]:
                    require(a[5] < b[3] or b[5] < a[3] or a[6] < b[4] or b[6] < a[4],
                            name + ": device and prefix ink do not overlap")
            continue
        require((29, 20, 18, "设备绑定") in rows and
                sum(row[3] == "解绑" for row in rows) == 1 and
                sum(row[3] == "返回" for row in rows) == 1 and
                not any(row[3] == "地址分区" for row in rows),
                name + ": fullscreen binding page replaces sidebar and retains actions")
        counts = [row for row in rows if re.fullmatch(r"总设备数:\d+台", row[3])]
        require(len(counts) == 1 and counts[0][:3] == (108, 20, 18),
                name + ": device count uses title size and device unit")
        header_ink = [row for row in inks[name] if row[1] < 22 and row[3] >= 0]
        require(len(header_ink) == 2 and all(2 <= row[4] <= row[6] <= 22 for row in header_ink),
                name + ": binding header ink fits top band")
        for i, a in enumerate(header_ink):
            for b in header_ink[i + 1:]:
                require(a[5] < b[3] or b[5] < a[3],
                        name + ": count and title do not overlap")
        fixture = re.fullmatch(r"subpage_unbind_count_(\d+)_time_(\d+)_button_(\d+)", name)
        boundary = re.fullmatch(r"binding_count_(\d+)_cycle_(\d+)", name)
        longest = re.fullmatch(r"subpage_binding_full_cycle_(\d+)_button_(\d+)", name)
        extreme_mac = re.fullmatch(r"subpage_unbind_mac_(\d+)_cycle_(\d+)_button_(\d+)", name)
        raw_count = int(fixture[1]) if fixture else int(boundary[1]) if boundary else \
                    20 if extreme_mac else 32 if longest else 0 if "empty" in name else 3
        count = min(raw_count, 20)
        column_rows = 10
        require(counts and counts[0][3] == f"总设备数:{raw_count}台",
                name + ": header preserves true bound count including defensive overflow fixture")
        require(not any(re.fullmatch(r"\d+/\d+", row[3]) for row in rows),
                name + ": no pagination indicator")
        if fixture or boundary or longest or extreme_mac:
            reference = f"subpage_unbind_count_{raw_count}_time_0_button_{fixture[3]}" if fixture else \
                        f"binding_count_{raw_count}_cycle_0" if boundary else \
                        f"subpage_unbind_mac_{extreme_mac[1]}_cycle_0_button_{extreme_mac[3]}" if extreme_mac else \
                        f"subpage_binding_full_cycle_0_button_{longest[2]}"
            require((output / f"after/{name}.pgm").read_bytes() ==
                    (output / f"after/{reference}.pgm").read_bytes(),
                    name + ": unbind table and action selection remain independent of timer")
        data = [row for row in rows if 22 <= row[1] < 140]
        require(data == [(14, 33, 11, "--")] if not count else len(data) == count * 2,
                name + ": all normal bindings retain only number and name")
        for i in range(count):
            left = 14 + 189 * (i // column_rows)
            baseline = 33 + 11 * (i % column_rows)
            device = data[i * 2:i * 2 + 2]
            name_width = 147
            expected_name = "W" * 31 if longest or extreme_mac else f"SW_MG_1_{i + 1}" if boundary else f"SW_{i + 1:02d}_WY_01-04"
            while width(expected_name) > name_width:
                expected_name = expected_name[:-1]
            require(device == [(left, baseline, 11, f"{i + 1}:"),
                               (left + 24, baseline, 11, expected_name)],
                    name + ": sequence and clipped name match device without MAC " + str(i + 1))
        data_ink = [row for row in inks[name] if 22 <= row[1] < 140 and row[3] >= 0]
        require(len(data_ink) == len(data), name + ": visible ink for every unbind data field")
        for row in data_ink:
            left = 14 if row[0] < 190 else 203
            require(left <= row[3] <= row[5] < left + 175 and
                    22 <= row[4] <= row[6] < 140,
                    name + ": device ink stays in its column above buttons " + row[7])
        for i, a in enumerate(data_ink):
            for b in data_ink[i + 1:]:
                require(a[5] < b[3] or b[5] < a[3] or a[6] < b[4] or b[6] < a[4],
                        name + ": number and name ink remain separate")
        action_ink = [row for row in inks[name] if row[7] in ("解绑", "返回") and row[3] >= 0]
        require(len(action_ink) == 2 and all(140 <= row[4] <= row[6] < 162 for row in action_ink) and
                all((6 <= row[3] <= row[5] < 189) if row[7] == "解绑" else
                    (195 <= row[3] <= row[5] < 378) for row in action_ink),
                name + ": both actions fit their independent bottom buttons")
        require(all(row[1] < 140 or row[3] in ("解绑", "返回") for row in rows),
                name + ": no device text enters the bottom action region")
    result = {"checks": len(checks), "failures": failures}
    (output / "param_page_contract.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Parameter page contracts: {len(checks)} checks, {len(failures)} failures")
    return result
