# Flash 压缩验证记录

日期：2026-10-07（America/New_York）。实际工程：`D:\蓝牙模块优化`。修改前 Git 基线：`2bde72c22e2c17d4e7d66468ebef006b9608219e`。

当前文字链路是 `ui_text_glyph → ui_menu_glyphs_11/14/16/18 → u8g2_DrawXBMP`，文字宽度也从这些位图字模计算。旧 `u8g2_SetFont(&u8g2, UI_FONT_CN)` 只设置未被当前绘图读取的字体上下文字段，却使 131,544 字节的 `u8g2_font_wqy14_t_gb2312a` 留在最终固件中。

本次只移除 `APP/yuying_TFT.c` 的 20 处无效 `SetFont` 调用并更新说明。字模、Logo、布局、数据源、BLE、UART 协议、日志、定时、息屏策略和绑定存储逻辑均未修改。`SetFontMode` 及位图模式、颜色的保存恢复保留；历史字库源码与 `UI_FONT_CN` 宏也保留，由工程原有 `--gc-sections` 回收未引用字库。没有改变优化选项、SDK 库、链接布局或 Flash 数据格式。

## 构建结果

使用实际 IDE 生成的 Make 规则和本机 WCH RISC-V GCC12 工具链。修改前在独立输出执行 `make -B -j2 all` 强制完整重编译，修改后同时验证独立输出和正式 `CH584_V1_0_1/obj` 完整构建。两阶段均成功，固件构建日志无 warning/error。

| 项目 | 修改前 | 修改后 | 减少 |
| --- | ---: | ---: | ---: |
| 链接器 FLASH Used Size | 342,484 B（74.66%） | 210,352 B（45.85%） | 132,132 B（129.04 KiB，38.58%） |
| 链接器 RAM Used Size | 66,040 B | 66,016 B | 24 B |
| Berkeley text | 341,940 B | 209,808 B | 132,132 B |
| Berkeley data | 544 B | 544 B | 0 |
| Berkeley bss | 45,152 B | 45,152 B | 0 |

Flash 容量按现有链接脚本为 458,752 B，压缩后余量 248,400 B。RAM Used Size 包含 RAM 执行段等，不等同于运行期最大栈余量。不要用 ELF/HEX 文件的磁盘大小衡量 Flash。

`nm` 和 map 确认：旧中文字库、`u8g2_SetFont` 及仅供它使用的字体信息读取代码已从最终链接中回收。除了字库本体，还减少无效调用及其依赖代码。

正式固件：`CH584_V1_0_1/obj/CH584M_TFT_HB.hex`；独立构建固件：`tools/screen_power_build/output/CH584M_TFT_HB.hex`。构建产物按现有规则忽略，不上传 GitHub。

## 功能回归

- 修改前先执行 `run_preview.py --phase before --scope unified --output output/flash_optimization_20261007`，冻结当前 UI 和两份字模头文件。修改后执行同入口的 `--phase after`。
- 对两阶段全部 309 个 `.pgm` 文件逐字节比较：用例集合相同、全部像素相同。两份字模头文件也完全一致。注意通用预览器的 unified 范围允许文字变化，默认不做前后逐像素比较；本任务额外独立比较全部文件，结果在 `flash_comparison.json`。
- 修改后 309 用例缺字、越界、数据改写、重绘差异、位图模式异常均为 0；文字布局 24,027 项、参数页面 52,501 项检查均为 0 失败。
- `tools/screen_uart_tests/run_tests.py`：61 项检查，0 失败。
- `tools/screen_power_tests/run_tests.cmd`：101 项检查，0 失败。
- 独立源码/map 审查确认：生产初始化将 `u8g2.font` 置空，但当前活动绘图不会读取它；仍使用的 `font_decode.is_transparent` 状态处理保持原样。

宿主预览有原有头文件编码警告 C4828；UART 宿主测试有原有 C4101（未使用局部变量）和 C4244（窄类型转换）警告。它们与固件构建分开记录，没有通过隐藏警告证明成功。

本次没有烧录、连接串口、操作 BLE 设备或控制硬件。烧录、真实屏幕、UART/BLE 联机、绑定保存/断电恢复及运行时性能均尚未验证。硬件验收最短流程：烧录新的正式 HEX，核对主页/菜单/消息提示，接入原传感器确认数值与 03/06 收发，再检查息屏唤醒及绑定保存后重启恢复。软件回归只能证明其覆盖输入下的行为。

## 本地证据

- `tools/screen_power_build/output/baseline_build.log`、`flash_baseline/CH584M_TFT_HB.map`：修改前完整构建与布局。
- `tools/screen_power_build/output/optimized_full_build.log`、正式 obj map：修改后完整构建与布局。
- `tools/ui_preview/output/flash_optimization_20261007/{before,after}/`：原始像素、绘制日志及源清单；同目录 `flash_comparison.json`：全部用例前后比较。
- `tools/ui_preview/output/flash_optimization_20261007/report.json`：修改后显示和布局检查。
- `tools/screen_uart_tests/out/` 与 `tools/screen_power_tests/`：现有宿主测试日志。

以上产物留在现有忽略目录，保留本地供复查。后续新增 u8g2 文本绘制时，应重新选择真正需要的字体并评估 Flash，不能假设宏保留就已初始化字体上下文。
