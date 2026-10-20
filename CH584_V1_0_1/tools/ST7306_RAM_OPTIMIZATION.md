# ST7306 4.2-inch low-RAM page buffer experiment

**Branch:** `opt/st7306-page-buffer-ram`. **NOT merged into main.**
This profile is specifically for the 4.2-inch ST7306 conversion path. The unchanged `main` branch remains the recommended baseline until the MounRiver build and display/BLE tests pass.

## Code-level RAM budget

| Static allocation | Before | Page-buffer branch |
|---|---:|---:|
| ST7306 U8g2 drawing buffer | 30,000 bytes (75 tile columns × 50 tile rows × 8) | 3,000 bytes (75 × 5 × 8) |
| Estimated static reduction | — | **27,000 bytes (26.37 KiB)** |
| `MEM_BUF` BLE heap (unchanged) | `BLE_MEMHEAP_SIZE = 1024*20` | 20,480 bytes |
| Linker `RAM` length (unchanged) | 96 KiB | 96 KiB |

The savings above represent the **U8g2 framebuffer only**, not a measured whole-firmware result. Actual `.bss` delta must be confirmed after a full MounRiver link and `.map` comparison. This branch enables `-ffunction-sections` and `-fdata-sections` in the existing project, plus `--gc-sections`, so unused full-framebuffer code/data should be discarded.

`SCREEN_ST7306_PAGE_BUFFER` is defined in `APP/include/yuying_TFT.h`:

- `1` (**this branch**): 3KB buffered U8g2 drawing; `UI_Control()` and standalone startup pattern use `u8g2_FirstPage/NextPage` loops. The whole virtual 600×400 screen is rendered and transferred in 10 page passes.
- `0`: restores original 30KB full buffer and `u8g2_ClearBuffer/SendBuffer` path.

**Never use `u8g2_SendBuffer` once on a page buffer:** doing so sends only the current part of the screen. Both of this project's frame paths are converted on this branch. If new drawing functions are added, they must follow the same picture-loop scheme.

## Why not simply change Link.ld to 128K?

The project still selects `CH584M` in `.template` and `.wvproj`, even though the SDK has CH585 declarations. CH584 has **96KB** physical SRAM, whereas CH585 has **128KB**. Changing linker size without positively identifying the board's silicon can cause RAM corruption. This page-buffer experiment **does not** alter `Link.ld`.

## Windows MounRiver validation

1. Clone or checkout the branch: `git fetch origin`, `git switch opt/st7306-page-buffer-ram`.
2. Open `CH584_V1_0_1/CH584M_TFT_HB.wvproj` in MounRiver Studio and build cleanly; save the complete compiler + linker output.
3. Compare `obj/CH584M_TFT_HB.map` and linker `Memory region Used Size` against a clean `main` build made with identical IDE/toolchain flags. Confirm old 30,000B `buf` is gone and 3,000B page buffer exists.
4. Independently power the screen MCU, verify **full** outer border, crosshair and two squares on the physical ST7306 screen. Compare for missing stripes, shifted pages and flicker.
5. Connect the baseboard UART3 at 115200 and test all pages, 20-channel data, menu keys, sleep/wake, reset, BLE scan/bind, and UART diagnostics. Do not use a successful link or static memory report as evidence of runtime safety.
6. Measure full-page refresh time and BLE dropped-scan/queue counters. With 10 redraw passes, the UI is likely slower and may increase SPI/CPU blocking in the BLE task loop. If unstable or too slow, set `SCREEN_ST7306_PAGE_BUFFER 0` and rebuild, or use the unchanged `main`.

## Verification evidence

A GitHub Actions host-GCC job performs **C syntax checks** for modified source modules, and a host linker GC probe checks that the isolated ST7306 framebuffer delta is exactly 27,000 bytes. The probe is **not a full RISC-V/MounRiver firmware build** and cannot verify display correctness.

## Potential follow-up improvements

- If ST7306 screen refresh is too slow, tune buffer page height to 10 tile rows (6,000 bytes, five passes; 24,000-byte reduction), or use partial-rectangle rendering with guarded panel memory writes.
- If actual chip reads CH585, a separate **hardware-confirmed** linker/profile update could use its 128KB SRAM. That changes allowed address range, not actual RAM consumption.
- Do not shrink BLE `MEM_BUF` from 20KB without collecting protocol stack minimum requirements and runtime scanning/binding stress results.
