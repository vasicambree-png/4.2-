# 4.2 寸显示屏固件 — ST7306 移植版

本仓库专用于 **CH58x + 4.2 寸 ST7306 300×400 显示屏**，与 [2.9 寸原版仓库](https://github.com/vasicambree-png/2.9-) 分离。原版 `main` 不受本仓库改动影响。

## 代码结构

- `CH584_V1_0_1/APP/`：4.2 寸屏幕初始化、单独上电测试图、尺寸开关和屏幕唤醒等修改。
- `CH584_V1_0_1/u8g2/`：ST7306 驱动修复和 30 KB 虚拟帧缓冲说明。
- `CH584_V1_0_1/tools/ST7306_42_BRINGUP.md`：接线、编译、烧录、验证步骤。
- `tools/prepare_st7306.ps1`：生成包含原工程全部 SDK 和字库依赖的 **4.2 寸完整本地工程**。

**注意：** GitHub 连接接口无法复制源仓库中的预编译 `.a` 二进制库和超大字库，因此本仓库保存独立的 4.2 寸修改源码，以 **2.9 寸主分支指定提交的只读副本** 补齐未修改依赖。生成脚本不会向 2.9 寸仓库提交代码。

## Windows 使用

在 PowerShell 中克隆本仓库后执行：

```powershell
cd 4.2-
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\prepare_st7306.ps1
```

脚本执行后，使用 **MounRiver Studio** 打开：

```text
.work_st7306\firmware\CH584_V1_0_1\CH584M_TFT_HB.wvproj
```

依次执行 Clean、Build。请先核对硬件确实使用 ST7306 控制器，烧录后观察边框 + 中央十字 + 两个角标的单独上电测试图。

## 当前边界

- 4.2 寸选择开关：`SCREEN_PANEL_ST7306_42 = 1`，位于 `APP/include/yuying_TFT.h`。
- 假设 4.2 寸物理屏为 300×400，默认 U8G2_R3 横屏方向。
- 已完成源码修订与 GitHub 远端文件校验；**未实际编译、烧录或实测**。
- 原来的 384×168 主页/子页坐标尚未做完整 400×300 重排，最终显示布局仍需适配。
- 源驱动及硬件手册均未验证是否匹配真实屏板；不要仅凭“4.2 寸”烧录。
