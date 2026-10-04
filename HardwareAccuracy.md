# 硬件准确性修复与验收

检测日期：2026-10-03（Asia/Shanghai）；Windows x64 / MSVC / Debug。

官方 Mooneye `mts-20260714-0944-31510e1` 的 40 个未经修改的 ROM，已从 **26 通过 / 14 失败** 提升至 **40 通过 / 0 失败**。2026-10-03 修复验收时，`GameBoyTests`、40 个独立 ROM 项与 `AudioFrontendTests` 共 **42/42** 通过，无跳过或预期失败项。

## 测试入口调整（2026-10-04）

全部 40 个 ROM 已合并至 `GameBoyTests`。入口为 `PixelLink::Tests::Gameboy::HardwareAccuracy::run()`，声明位于 `TestSuites.hpp`，由现有无参数 `main()` 调用。独立的 `HardwareAccuracyTests` 程序与重复运行原始八个 ROM 的 `ROMTest` 已移除。

ROM 清单统一放在 `tests/PixelLink/GameBoy/HardwareAccuracy.cpp`，程序自动依次测试全部条目，使用现有 `Test::run` 格式逐项报告结果。某项失败后继续运行其它 ROM，最终以非零状态退出；缺失 ROM 作为失败报告。

重新完整构建后的 `GameBoyTests` 与 `AudioFrontendTests` **2/2 CTest 项通过**；统一入口中的 **40/40 ROM 各执行一次并通过**。另行验证了缺失首个 ROM 的情况：该项报告失败，剩余 39 项全部执行且通过，程序返回 1。验证后原始 ROM 已恢复并重新核对哈希。

当前结果：[integrated-tests.xml](out/build/hardware/integrated-tests.xml) 与 [integrated-tests.log](out/integrated-tests.log)；逐项输出见 [LastTest.log](out/build/hardware/Testing/Temporary/LastTest.log)。

40 个 ROM 的 SHA-256 已重新核对，与 [manifest.json](assests/roms/mooneye/manifest.json) 全部一致。测试使用真实指令和硬件访问；未修改 ROM，也未添加按 ROM 名称区分的处理。

| 类别 | 修复前通过 | 修复后通过 | 总计 |
| --- | ---: | ---: | ---: |
| CPU / Interrupt | 7 | 7 | 7 |
| Timer | 14 | 14 | 14 |
| DMA | 2 | 6 | 6 |
| PPU | 3 | 12 | 12 |
| Serial | 0 | 1 | 1 |

## 已完成的修复

- **CPU 与核心时钟**：取指、操作数、间接访存、绝对地址访问、读改写、栈访问和中断入口按机器周期推进硬件。核心不再在指令结束后补推进同一段时间。
- **OAM DMA**：启动延迟、重启期间旧传输的延续、160 字节完成边界、源地址镜像、CPU 总线限制及 FF46 读取。
- **PPU**：LCD 打开后的特殊首行、正常扫描线模式切换、SCX/精灵/窗口对 Mode 3 时长的影响、STAT 共享中断线、LYC 比较、VBlank 边界及第 153 行 LY 提前归零。
- **PPU 读写端口**：VRAM/OAM 分开判定读写权限，覆盖 Mode 2/3 边界及 OAM 扫描结束前短暂允许写入的窗口；LY 的 CPU 写入被忽略。
- **Timer**：溢出等待与重载窗口分开处理，修正重载期间 TIMA/TMA 的写入行为。此修复已包含在 26/40 基线中，本轮继续保持全部 14 项 Timer 验收通过。
- **Serial**：新增 DMG 串口传输设备，支持内部时钟、断线输入、外部逐位输入输出、完成中断、取消和重启。核心使用 DMG ABC 启动后的串口时钟相位，DIV 写入不重置串口时钟。
- **Windows 前端部署**：CMake 在链接 `FrontendTests` / `AudioFrontendTests` 后，将所选共享 SDL 目标的 DLL 自动复制到对应程序目录。部署副本与构建出的 DLL 哈希一致，缺失 DLL 导致的启动错误已解决。

## 回归覆盖

新增或更新了 CPU 访存/栈/中断周期、DMA 启动/重启/结束、LCD 开启与模式边界、VRAM/OAM 读写差异、LY 寄存器、Timer 逐周期重载和串口逐位传输测试。原有指令、渲染、映射器、RTC、存档、输入和 APU 核心测试继续通过。

`AudioFrontendTests` 使用 SDL dummy 音频驱动，验证 APU 样本经前端提交给 SDL、初始化/关闭及重新初始化。它证明音频集成通路可以运行，尚未验证扬声器实际发声或声音是否准确。`FrontendTests.exe` 已重新编译并部署 DLL；本轮未运行需要手动关闭窗口的交互游戏测试。

## 全部 ROM 结果

| ROM（Accuracy/ 前缀省略） | 类别 | 修复前 | 修复后 |
| --- | --- | --- | --- |
| `add_sp_e_timing` | CPU / Interrupt | PASS | PASS |
| `div_timing` | Timer | PASS | PASS |
| `ei_sequence` | CPU / Interrupt | PASS | PASS |
| `ei_timing` | CPU / Interrupt | PASS | PASS |
| `halt_ime0_ei` | CPU / Interrupt | PASS | PASS |
| `halt_ime0_nointr_timing` | CPU / Interrupt | PASS | PASS |
| `halt_ime1_timing` | CPU / Interrupt | PASS | PASS |
| `ld_hl_sp_e_timing` | CPU / Interrupt | PASS | PASS |
| `oam_dma_start` | DMA | FAIL | PASS |
| `oam_dma_restart` | DMA | FAIL | PASS |
| `oam_dma_timing` | DMA | FAIL | PASS |
| `oam_dma/basic` | DMA | PASS | PASS |
| `oam_dma/reg_read` | DMA | PASS | PASS |
| `oam_dma/sources-GS` | DMA | FAIL | PASS |
| `ppu/hblank_ly_scx_timing-GS` | PPU | FAIL | PASS |
| `ppu/intr_1_2_timing-GS` | PPU | PASS | PASS |
| `ppu/intr_2_0_timing` | PPU | PASS | PASS |
| `ppu/intr_2_mode0_timing` | PPU | FAIL | PASS |
| `ppu/intr_2_mode0_timing_sprites` | PPU | FAIL | PASS |
| `ppu/intr_2_mode3_timing` | PPU | FAIL | PASS |
| `ppu/intr_2_oam_ok_timing` | PPU | FAIL | PASS |
| `ppu/lcdon_timing-GS` | PPU | FAIL | PASS |
| `ppu/lcdon_write_timing-GS` | PPU | FAIL | PASS |
| `ppu/stat_irq_blocking` | PPU | PASS | PASS |
| `ppu/stat_lyc_onoff` | PPU | FAIL | PASS |
| `ppu/vblank_stat_intr-GS` | PPU | FAIL | PASS |
| `timer/div_write` | Timer | PASS | PASS |
| `timer/rapid_toggle` | Timer | PASS | PASS |
| `timer/tim00` | Timer | PASS | PASS |
| `timer/tim00_div_trigger` | Timer | PASS | PASS |
| `timer/tim01` | Timer | PASS | PASS |
| `timer/tim01_div_trigger` | Timer | PASS | PASS |
| `timer/tim10` | Timer | PASS | PASS |
| `timer/tim10_div_trigger` | Timer | PASS | PASS |
| `timer/tim11` | Timer | PASS | PASS |
| `timer/tim11_div_trigger` | Timer | PASS | PASS |
| `timer/tima_reload` | Timer | PASS | PASS |
| `timer/tima_write_reloading` | Timer | PASS | PASS |
| `timer/tma_write_reloading` | Timer | PASS | PASS |
| `serial/boot_sclk_align-dmgABCmgb` | Serial | FAIL | PASS |

## 当前边界与下一目标

本次完成的是所选 DMG 验收子集的全部已知失败。PPU 仍逐行绘图，完整像素 FIFO/fetcher、任意扫描线中途寄存器变化、完整 Mooneye 测试集以及 CGB/SGB 专属行为尚未完成验证。串口启动状态采用预设相位，尚未执行实际 boot ROM，也未实现前端联机功能。

**下一目标：实际游戏的声音验收，再扩展 APU 硬件准确性。** 先确认本地 ROM 在真实音频设备上能发声，检查静音/失真、左右声道与长时间播放；随后覆盖 DIV 同步、wave RAM 访问和触发行为等高级 APU 项。验收时同时复跑当前 40 个 ROM 与核心测试，保持现有通过结果。

## 复跑与产物

构建目录：`out/build/hardware`。2026-10-03 的修复验收结果：[repair-final.xml](out/build/hardware/repair-final.xml) 与 [repair-final.log](out/repair-final.log)。构建产物不进入版本控制。

当前可运行的前端：[FrontendTests.exe](out/build/hardware/FrontendTests.exe)，配套 DLL 在同目录。音频自动检查：[AudioFrontendTests.exe](out/build/hardware/AudioFrontendTests.exe)。

在已配置的 C++ 开发环境中运行：

```powershell
cmake --build out/build/hardware --clean-first --target GameBoyTests FrontendTests AudioFrontendTests
ctest --test-dir out/build/hardware -R '^(GameBoyTests|AudioFrontendTests)$' --output-on-failure --output-junit integrated-tests.xml
```

本机 Ninja/MSVC 的头文件依赖输出使用中文；本轮采用完整重新编译，以避免头文件改动后残留旧布局的目标文件。其它生成器可以按自身的配置正常构建；多配置构建需加 `--config Debug` 和 CTest 的 `-C Debug`。

ROM 分组、来源和运行器说明见 [ROM README](assests/roms/mooneye/README.md)；项目进度见 [ProjectPlan.md](ProjectPlan.md)。
