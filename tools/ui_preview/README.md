# CH584 384×168 UI 离线预览

在工程根目录运行：

```powershell
python tools/ui_preview/run_preview.py
```

当前主页两页统一88×44 Logo、18px数据文字和22px行距，顶栏14px。
2026-10-08：每翼两条斜线；第一页无线图标位于电压左侧，两者同行，下方为分站号和右对齐本机号；
电压通常16px，超长字符串回退14px；普通电压旁无线图标29×21，长电压旁采用21×17紧凑图标，间隔3px；第二页右上角为状态:开机/关机，
下方为已绑定数字、独立间隔4px的“台”和已用通道。两页信息栏左锚108、右锚366，各向中间内收6px；第二页
不显示电压、无线图标、报警。冒号仍为4px窄字模，最长数字完整显示。
主页范围检查真实文字墨迹、单元格边界及内部碰撞，两页等待/真实零值/部分接收独立检查。
无线图标是Wi-Fi外形，读取主控`UI_main.Lora_rssi`；2026-10-09按用户提供的STM32L0 HEX确认发送8–11，
采用用户确认的8斜杠、9弱、10中、11强，其他值均显示带斜杠。旧0–3不再表示有效档位。
主控输入阈值80/96/110已从HEX核实；图片顺序是用户确认约定，实际dBm与联网状态尚未验证。
本轮复验：`python -B tools/ui_preview/run_preview.py --scope home --output output/wireless_hex_20261009 --phase after`。
覆盖正常/紧凑图标的8–11、边界7/12和旧0–3、255回退，记录见[HEX无线显示映射](../WIRELESS_HEX_DISPLAY.md)。
资源由`tools/generate_wireless_assets.py`生成；“台”由已有菜单字模生成器补齐四种字号。
此前参考图轮次将无线图标改为三层圆弧与独立圆点，当时为17×13；弱/中/强档分别保留
一/二/三道弧线，无信号保留斜线；该历史轮次未知值曾显示问号。该次预览在
`output/home_wifi_arc_20261008/`，`after/home_wifi_3_3x.png`为强信号图标示例。
本轮独立预览在`output/home_wifi_crossed_fallback_20261008/`，复验命令：

```powershell
python -B tools/ui_preview/run_preview.py --scope home --output output/home_wifi_crossed_fallback_20261008 --phase after
```

工具使用完整 `CH584_V1_0_1/APP/yuying_TFT.c` 和实际工程头文件，直接编译工程的字体解码、UTF-8 解码、位图、矩形、圆角和线条绘制算法。只将显示端点接到本地 384×168 单色像素数组，并把 GPIO/SPI/延时端点设为空操作。源码仅在生成的宿主副本中将 MSVC 不接受的未使用空数组调整为一个零字节。

第一次运行会保留 `output/yuying_TFT.session_before.c`，后续运行不会覆盖此快照。`before` 从该快照编译，`after` 从当前 UI 源码编译。历史 `obj/display_verify` 不会被修改。

MSVC 默认环境脚本是当前机器已有的 `D:\visual studio\visual studio2026\VC\Auxiliary\Build\vcvars64.bat`。其他机器可在进程环境设置 `UI_PREVIEW_VCVARS` 指向其 `vcvars64.bat`。工具不修改系统 PATH，也不安装软件。Python 工具仅使用标准库。

结果统一写到忽略的 `output/`：

- `after/home_example.png`、`after/home_example_4x.png`：第 1 页参考示例数据和整数倍放大；`after/home_page1_3x.png`、`after/home_page2_3x.png`：两页正常数据的 3 倍最近邻预览。
- `before/`、`after/`：各用例图片、实际绘制日志、逐用例检查、编译日志和中间文件。
- `report.md`、`report.json`：缺字、屏幕边界、输入/通道/绑定数据不改写、连续刷新/独立重画一致性，以及保留菜单和提示页的前后逐像素对比。

`python tools/ui_preview/run_preview.py --phase after` 只更新当前渲染；`--report-only` 使用已有结果生成报告和 PNG。

二三级页面修改使用独立范围和输出目录：

```powershell
python -B tools/ui_preview/run_preview.py --scope menus --output menu_output --phase both
```

`--scope` 默认是 `home`，保留上述主页工作流和 `output/` 默认路径。`menus` 默认输出到 `menu_output/`；显式 `--output` 相对于 `tools/ui_preview/`，必须是该目录内的独立子目录。两个范围使用各自的会话快照，已有快照永不覆盖，菜单运行不会写入历史 `output/`。

本轮菜单基线已保存为 `menu_output/yuying_TFT.session_before.c`。需要固定来源时，加上 `--baseline-sha256 <完整SHA256>`；快照缺失或摘要不一致会立即停止，避免将修改后的源码当成基线。`prepare_host.py` 只在对应源码引用资源时复制 `ui_home_assets.h` 或 `ui_menu_assets.h`，before 使用该范围内冻结的资源副本，after 使用当前资源。

菜单范围覆盖全部合法焦点、子页按钮、空/满绑定、绑定列表轮显全部页、29 字节最长名称、上传地址 0/121/122/65535、地址/组网/功率/亮屏时长/告警数量极值、RSSI 和电压极值、20 通道空/稀疏/完整数据及交替页面。每次绘图都检查输入数据及消息码不改写、位图透明模式恢复，并对比连续刷新和独立重绘；每个 PNG 另生成 `*_3x.png`。

边界统计同时检查黑字和 `draw_color=0` 的反白文字。宿主启动时用真实字库绘制两个故意越界的反白字符，分别验证右侧越界与完全位于屏幕左侧的情况；探针只验证检查器，不计入固件页面的越界统计。`summary.json` 单独报告探针失败数量。

`menus` 允许菜单页面视觉变化，要求所有 `home_*`、`message_*`、`save_message_tick*`、`power_off_message_tick10` 和 `restart_confirmation` 在 before/after 间逐像素一致。名称、信号和电压页以 `chu_num2=0/1/2/65535` 绘制，各页码均显示全部20路，检查页码不能改变通道集合。

2026-10-07：三级页面移除顶部标题；20路页面采用11px正文、13px行距，编号和名称/读数在各自区域居中，底部中央保留返回按钮。新增跨菜单残留 `re_flag=3/4/5`、菜单7返回主页、末行下伸字符回归。本轮快照和预览在 `output/detail_return_20261007/`，统一布局复验在 `output/detail_return_unified_20261007/`；原因、固件和验收步骤见 [三级页面返回与布局修正](../DETAIL_PAGE_FIX.md)。

同日后续恢复紧凑顶栏和右上角返回：正文11px、行距14px，两数据框183×142px；当时检查器要求14px标题、11px子标签和右上角14px返回文字。此次历史前后对照在 `output/compact_detail_20261007/`，统一复验在 `output/compact_detail_unified_20261007/`。

2026-10-08 后续按反馈将所有三级页标题统一为18px。20路数据页标题的点阵起点、两数据框y=22/183×142px、14px行距和右上角返回按钮位置保持不变；18px字体基线按同一y=3点阵起点补偿。当前菜单合同检查覆盖三级标题字号、标题墨迹与数据区边界；地址、上传和其他设置页的保存按钮文案为“保存并返回”，主控端动作未改。当前预览输出见 `output/third_level_title_20261008/`。

分阶段运行可使用 `--phase before` 或 `--phase after`，然后通过下面命令合并比较报告：

```powershell
python -B tools/ui_preview/run_preview.py --scope menus --output menu_output --phase both --report-only
```

before 的页面问题会保留在报告中供诊断；after 的缺字、越界、数据改写、重绘差异、位图状态变化、检查器探针失败或保留页面不一致会使命令返回非零。图片转换只处理本次 `cases.tsv` 记录的用例，目录中的旧图片不会混入比较。

报告另外列出宿主编译警告和错误，不把桌面 MSVC 编译与固件 WCH GCC 构建混为一项。本机现有 `u8g2.h` 字库声明前的 GBK 注释在 `/utf-8` 下产生 C4828 编码警告；工具不修改该固件头文件。

离线预览不能证明固件完整编译、真实 UART/BLE/Flash/刷新调度、烧录或屏幕实物表现。检查只覆盖 UI 函数在给定输入下的绘制结果，不会操作串口或硬件。

## 统一文字与等待数据布局

本次同时修改首页、菜单及提示字体，使用独立范围，所有页面允许字体变化：

```powershell
python -B tools/ui_preview/run_preview.py --scope unified --output unified_text_output --phase after
```

`unified` 包含菜单范围的焦点、极值和轮显用例，另加已绑定但尚未收到广播、
收到真实零值、部分通道收到数据、末行名称下伸笔画用例。运行器自动执行
`check_unified_text.py`，从真实 C 字模渲染记录验证状态居中、左右信息间距、
20通道固定标签、数据等待与合法零值、名称在遥测之前显示。
结果见 `text_contract.json`，失败会使整体命令返回非零。

绘图测试同时观察 u8g2 字体和本项目位图字模入口。字体字号表来自当前生成头文件，
不以测试程序另绘一套字体代替固件；普通文本缺字及反白位图的边界均计入检查。
两页状态/统计位于Logo右侧顶部，数据框位置、字号与行高统一且延伸到底部。
before通过实际绘图识别旧版布局，after要求新的顶栏及网格布局。
历史 before/after 字体差异是本次预期变化，`unified` 不要求旧页面逐像素相同。
`--phase both` 可保留视觉对照，before 的旧布局问题只作诊断。

全部输出、源码快照及编译日志留在忽略目录 `unified_text_output/`，不上传预览缓存。

## 三级页面恢复两页

```powershell
python -B tools/ui_preview/run_preview.py --scope unified --output param_pages_output --phase after
```

`check_param_pages.py`从实际绘图记录检查每页10组编号、双列五行、页码/确认提示、
单元格墨迹边界及内部文字碰撞。两页均覆盖等待数据、真实零值和部分接收。
模拟主控帧解析后页面状态的进入、翻页、退出、重新进入检查只验证绘图响应，
不执行帧解析或模拟物理确认键。
本轮显示行为和实机步骤见[三级页面两页显示说明](../三级页面两页显示说明.md)。
