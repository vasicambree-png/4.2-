# 绑定设备两列与保存返回

2026-10-08，实际项目 `D:\蓝牙模块优化`。

## 变更

- `APP/yuying_TFT.c` 的绑定扫描列表使用左右两列，左侧1–3、右侧4–6，每条名称前显示 `1:` 等序号。保持原扫描缓存顺序、11px字模和最多6条缓存；长名称按既有函数裁剪到列宽，不改写缓存。保存/返回按钮保留原焦点编号。
- `APP/Usart3_task.c` 中有效0x05请求完成保存与应答后，仅当当前页为menu_rank=3、rank2_addr=2、re_flag=2时，返回menu_rank=2、rank2_addr=2、rank3_addr=2、re_flag=0。立即设置SCAN_MODE_DATA并更新息屏页面策略；原成功/无绑定/失败提示均保留，提示结束后显示上级菜单。其他页面的保存请求不改页；非法请求不保存、不跳转。
- UI绘图回归补充1–6台与两种按钮焦点，核对序号、跨列顺序、字号、最长名称裁剪与文字碰撞；UART回归执行实际解析器与消费路径，外设/Flash为桩。

## 验证

- WCH固件构建成功，本轮两个修改源的对象、ELF与HEX已更新，源哈希稳定；构建日志0 warning、0 error。FLASH 212364/458752 B，RAM 66064/98304 B。
- 真实C绘图347个场景：缺字、越界、数据改写、重绘不一致、位图状态异常均0；布局44503项检查，0失败。MSVC已有36条C4828注释编码警告，0错误。查看正常6条设备及最长名称的原生/3倍像素预览。
- 真实UART解析软件回归172项，0失败，覆盖保存成功/空/失败、非法LEN/SUM、不相关页面、立即退出绑定扫描及返回后息屏超时。宿主存在已有C4101、C4244警告；该检查不证明实际Flash写入成功。

预览：`tools/ui_preview/output/binding_columns_20261008/`；固件：`tools/screen_power_build/output/CH584M_TFT_HB.hex`。

```powershell
& 'C:/Users/16048/AppData/Local/Programs/Python/Python314/python.exe' -B tools/ui_preview/run_preview.py --scope menus --output output/binding_columns_20261008 --phase after
& 'C:/Users/16048/AppData/Local/Programs/Python/Python314/python.exe' -B tools/screen_uart_tests/run_tests.py
& tools/screen_power_build/build_firmware.ps1
```

烧录、物理按键、UART/BLE联机及实屏均**尚未验证**。主控仍通过0x01帧拥有后续页面控制权，若主控在保存后持续重发旧扫描页，会再次进入旧页；当前仓库无该主控按键工程。最短实机验收：烧录本轮HEX，进入绑定页核对1–6序号/两列及两种焦点，按保存查看结果提示结束后停在上一级设备绑定菜单；检查主控随后发帧不覆盖返回状态，并核对保存后重启的绑定恢复。
