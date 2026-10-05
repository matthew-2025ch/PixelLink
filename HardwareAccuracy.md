# 硬件准确性修复与验收

检测日期：2026-10-03（Asia/Shanghai）；Windows x64 / MSVC / Debug。

官方 Mooneye `mts-20260714-0944-31510e1` 的 40 个未经修改的 ROM，已从 **26 通过 / 14 失败** 提升至 **40 通过 / 0 失败**。2026-10-03 修复验收时，`GameBoyTest`、40 个独立 ROM 项与 `AudioFrontendTest` 共 **42/42** 通过，无跳过或预期失败项。

## 测试入口调整（2026-10-04）

全部 40 个 ROM 已合并至 `GameBoyTest`。入口为 `PixelLink::Test::GameBoy::HardwareAccuracy::run()`，声明位于 `TestSuites.hpp`，由现有无参数 `main()` 调用。独立的 `HardwareAccuracyTest` 程序与重复运行原始八个 ROM 的 `ROMTest` 已移除。

ROM 清单统一放在 `Test/PixelLink/GameBoy/HardwareAccuracyTest.cpp`，程序自动依次测试全部条目，使用现有 `Test::run` 格式逐项报告结果。某项失败后继续运行其它 ROM，最终以非零状态退出；缺失 ROM 作为失败报告。

重新完整构建后的 `GameBoyTest` 与 `AudioFrontendTest` **2/2 CTest 项通过**；统一入口中的 **40/40 ROM 各执行一次并通过**。另行验证了缺失首个 ROM 的情况：该项报告失败，剩余 39 项全部执行且通过，程序返回 1。验证后原始 ROM 已恢复并重新核对哈希。

2026-10-04 验收记录：[integrated-tests.xml](out/build/hardware/integrated-tests.xml) 与 [integrated-tests.log](out/integrated-tests.log)；逐项输出见 [LastTest.log](out/build/hardware/Testing/Temporary/LastTest.log)。

## 前端音频入口调整（2026-10-05）

音频集成检查已合并至 `FrontendTest`，入口为 `PixelLink::Test::Frontend::AudioTest::run()`，在 `TestSuites.hpp` 中声明。前端主入口先运行自动音频检查，再运行需要手动关闭的游戏窗口测试；独立 `AudioFrontendTest` 构建目标已移除。

自动检查临时使用 dummy 音频驱动。通过作用域清理关闭其音频子系统，并恢复之前的驱动选择；正常返回或断言异常都会执行清理，SDL 初始化失败时也恢复驱动。后续游戏窗口按原来的音频配置初始化。本次调整完成重新构建，尚未复跑交互窗口或实际听音验收。

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
- **Windows 前端部署**：CMake 在链接 `FrontendTest` / `AudioFrontendTest` 后，将所选共享 SDL 目标的 DLL 自动复制到对应程序目录。部署副本与构建出的 DLL 哈希一致，缺失 DLL 导致的启动错误已解决。

## 回归覆盖

新增或更新了 CPU 访存/栈/中断周期、DMA 启动/重启/结束、LCD 开启与模式边界、VRAM/OAM 读写差异、LY 寄存器、Timer 逐周期重载和串口逐位传输测试。原有指令、渲染、映射器、RTC、存档、输入和 APU 核心测试继续通过。

2026-10-03/04 的独立音频检查使用 SDL dummy 音频驱动，验证 APU 样本经前端提交给 SDL、初始化/关闭及重新初始化，并通过验收。该检查现通过 `Frontend::AudioTest::run()` 合并到 `FrontendTest`。自动检查尚未验证扬声器实际发声或声音是否准确。

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

此处原定的 APU 扩展与独立窗口现已实现，2026-10-05 的结果见下方更新。后续可以开展第二阶段局域网控制；完整 PPU FIFO、CGB/SGB、不同芯片修订的模拟音频差异仍属于后续准确性扩展。

## APU 与独立 SDL 应用（2026-10-05）

APU 的帧序列器改为由真实 Timer 的 DIV bit 4 下降沿驱动（内部系统计数器 bit 12），
包括 CPU 写 DIV 产生的边沿；Timer 与音频振荡器按同一 T-cycle 推进。独立 APU 的测试模式
仍提供本地分频器，关电时保持分频相位，重新上电从序列器 step 0 开始。

补齐 DMG 关电期间长度计数器保留及写入、额外长度时钟、零长度触发重载、包络时钟重载、
扫频减法历史和零位移溢出检查、波形样本缓冲与启动延迟、运行期间 wave RAM 访问窗口，
以及两周期预取阶段的波形重触发损坏。CPU 可访问 RAM 的窗口与重触发损坏窗口分别处理。
DAC 数字值转换和高通滤波现在随输出采样率计算，并在 DAC 全关时输出静音。

新增 12 个未经修改的 Blargg `dmg_sound` ROM，来源固定为
[`retrio/gb-test-roms` c240dd7](https://github.com/retrio/gb-test-roms/tree/c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15/dmg_sound)。
它们通过正常 CPU 执行，并按作者的 A000–A004 内存协议读取结果；没有 ROM 名称对应的硬件捷径。
清单和 SHA-256 见 [manifest.json](assests/roms/blargg-dmg-sound/manifest.json)。测试在构建目录的
新临时目录中复制 ROM 并产生存档，不修改游戏库的原始 ROM 或存档。

| Blargg DMG APU ROM | 结果 |
| --- | --- |
| 01-registers | PASS |
| 02-len ctr | PASS |
| 03-trigger | PASS |
| 04-sweep | PASS |
| 05-sweep details | PASS |
| 06-overflow on trigger | PASS |
| 07-len sweep period sync | PASS |
| 08-len ctr during power | PASS |
| 09-wave read while on | PASS |
| 10-wave trigger while on | PASS |
| 11-regs after power | PASS |
| 12-wave write while on | PASS |

实现依据参考 [Pan Docs / Audio Details](https://gbdev.io/pandocs/Audio_details.html)、
[Audio Registers](https://gbdev.io/pandocs/Audio_Registers.html) 及原始 ROM 验收结果。
上述验收覆盖 CPU 可观察的 DMG 行为，不代表所有芯片修订、CGB/SGB 或模拟音频细节完全一致。

独立 `PixelLink` 程序提供中文游戏库，递归扫描统一的 `assests/roms`；从其它位置导入或打开的 ROM 会复制到游戏库。
支持搜索、鼠标/键盘选择、多文件原生对话框导入、拖放、进入/退出、暂停、静音和电池存档。
不同游戏使用新的模拟器实例；保存使用完整临时文件替换。保存失败时保留当前游戏并阻止退出。
操作和构建说明见 [README.md](README.md)。

Release 验收：`GameBoyTest` 中 **194/194** 项通过，包括原有 **40/40 Mooneye** 和
新增 **12/12 Blargg APU ROM**；`DesktopTest` 的自动音频、导入、窗口、存档和本地游戏检查通过。
CTest **2/2**，结果保存在 `out/build/release/final-tests.xml` 和 `out/release-validation.log`。
Debug 同样 **194/194** 核心项、**40/40 Mooneye**、**12/12 APU ROM** 与自动窗口检查通过，
CTest **2/2**。原生 Windows 视频后端和 WASAPI 真实音频设备的隐藏窗口检查全部通过。
结果见 `out/debug-validation.log`、`out/native-device-validation.log`。

本地《For the Frogs the Bell Tolls》生成有效的左右声道样本，PCM 峰值约 0.753，
通过 SDL 成功提交给 Windows WASAPI 真实设备。《Super Breakout》当前标题画面样本为静音，
已记录这一实际结果，没有将静音数据当作成功发声。自动设备检查验证初始化、样本提交和关闭，
无法替代人耳判断或确认扬声器的实际音量；听感和长期播放仍可在独立应用中进一步验收。

正式应用：`out/build/release/PixelLink.exe`，SDL3.dll 已自动部署到同目录。
完整流程测试包含中文路径、递归扫描、同名导入、现有存档保护、按键释放、暂停音频、
新核心切换、无效 ROM、存档写入失败与恢复、关闭应用前保存和实际游戏库截图。
CPU 测试访问也改为显式测试支持，Release 不再依赖 `_DEBUG` 暴露寄存器，断言继续使用 `CHECK`。

## 复跑与产物

2026-10-03/04 的历史构建目录为 `out/build/hardware`，修复验收结果为
[repair-final.xml](out/build/hardware/repair-final.xml) 与 [repair-final.log](out/repair-final.log)。
本次构建目录为 `out/build/release` 和 `out/build/verified`；构建产物不进入版本控制。

当前独立应用：[PixelLink.exe](out/build/release/PixelLink.exe)，配套 SDL3.dll 在同目录。
`FrontendTest` 保留为交互测试入口；自动窗口流程使用 `DesktopTest`。

在已配置的 C++ 开发环境中运行：

```powershell
cmake --build out/build/release --target PixelLink GameBoyTest DesktopTest FrontendTest
ctest --test-dir out/build/release -R '^(GameBoyTest|DesktopTest)$' --output-on-failure
out/build/release/PixelLink.exe
```

本机 Ninja/MSVC 的头文件依赖输出使用中文，当前构建通过
`PIXELLINK_MSVC_INCLUDE_PREFIX` 覆盖乱码识别前缀，恢复头文件依赖跟踪；首次验收也进行了完整重编译。
具体配置见 README。其它语言或生成器保持默认；多配置构建需加对应的 `--config` 和 CTest 的 `-C`。

ROM 分组、来源和运行器说明见 [ROM README](assests/roms/mooneye/README.md)；项目进度见 [ProjectPlan.md](ProjectPlan.md)。
