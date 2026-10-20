# 绑定全屏与数据子标题调整

2026-10-08，实际项目 `D:\蓝牙模块优化`，基线 UI 源 SHA256：`eb4d9303e67c545c675807308c5fdaccca1fd06c8c18dd7354ee1d2f54abbf88`。

## 变更范围

- `CH584_V1_0_1/APP/yuying_TFT.c`：设备绑定的扫描三级子页（菜单2、re_flag=2）使用与解绑一致的全屏外框，单列保留最近6条扫描名称；名称区域354px，底部保留保存/返回及原焦点编号。二级页面残留re_flag=2时仍沿用原右栏布局。
- 解绑子页顶部计数改为14px，与“设备绑定”同字号，显示“总设备数:N台”。32台字符串宽92px，现有x108位置可容纳，且与解绑按钮分离。名称/MAC两列、每屏10台、4秒时基轮显保持原逻辑。
- 20路名称、信号、电压页只删除“名称/信号/电压”子标题，按用户确认保留左上角“信息汇总/安装调试”、右上返回按钮、全部数据及原位置。
- `tools/ui_preview/host_render.c` 与 `check_param_pages.py`：更新对应绘图契约，补充二级残留扫描状态、六条最长名称及顺序、设备计数字号/单位和顶部文字间距检查。协议、BLE解析、Flash、调度和字模资源未修改。

## 本轮验证

- 运行当前 `tools/screen_power_build/build_firmware.ps1`，使用实际IDE生成规则及WCH工具链编译受影响源、链接并生成HEX成功；本轮日志0 warning、0 error，源码哈希稳定。FLASH 212152/458752 B，静态RAM 65920/98304 B。
- 实际C绘图335个场景，缺字、越界、数据改写、重绘不一致、位图模式及首页契约异常均为0；参数页与相关界面43731项检查，0失败。宿主MSVC有36条已有C4828注释编码警告，0错误。
- 改动前冻结源与资源；按页面状态比较335个前后场景：75个请求涉及的场景有预期变化，其余260个逐像素一致，包括首页、消息、其他菜单和二级残留扫描状态。
- 已查看原生尺寸及3倍最近邻预览，覆盖空列表、最长扫描名、32台解绑计数和按钮焦点。

预览输出：`tools/ui_preview/output/binding_fullscreen_20261008/`，基线和结果属于本轮，按项目规则忽略。

```powershell
& 'C:/Users/16048/AppData/Local/Programs/Python/Python314/python.exe' -B tools/ui_preview/run_preview.py --scope menus --output output/binding_fullscreen_20261008 --phase after --baseline-sha256 eb4d9303e67c545c675807308c5fdaccca1fd06c8c18dd7354ee1d2f54abbf88
& tools/screen_power_build/build_firmware.ps1
```

HEX：`tools/screen_power_build/output/CH584M_TFT_HB.hex`，SHA256：`FEDF2C4FCB1796CF81341BD4DD07FC29CD0566AE9576C43D40E0978E6887C20A`。

烧录、UART/BLE联机、实屏与物理按键效果均**尚未验证**。最短实机验收：烧录本轮HEX，进入绑定扫描页查看全屏及保存/返回焦点；进入解绑页查看“总设备数:N台”、名称/MAC及轮显；检查20路三个数据页只移除了子标题，退出重入及其他页面正常。
