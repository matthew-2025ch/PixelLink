# PixelLink

Windows / SDL3 的本地 Game Boy 模拟器，支持 ROM 游戏库、声音、键盘输入和电池存档。

## 运行

本机 Visual Studio 构建的程序：`out/build/local-msvc/Release/PixelLink.exe`；
下方 Ninja 构建示例的程序位于 `out/build/release/PixelLink.exe`。将同目录的 `SDL3.dll`
保留在程序旁边，双击即可打开中文游戏库，无需通过测试程序运行游戏。

默认递归扫描项目的 `assests/roms/`，包括所有子文件夹；导入的游戏也统一存放在这里。
支持 `.gb`、`.gbc`、`.rom`、`.bin`，扩展名不区分大小写；存档、清单和说明文件不会当作 ROM。
每个子文件夹中的 ROM 都会显示，列表保留相对路径以区分同名文件。

- 单击选择，双击或按 Enter 启动；方向键、Page Up/Down 和滚轮浏览。
- 点击搜索框输入游戏名称或子文件夹；Esc 清空搜索，F5 刷新列表。
- 点击“导入 ROM”或按 Ctrl+O 打开原生文件选择窗口，可批量选择。
- 也可以将 ROM 文件拖到游戏库窗口。
- 可从电脑任意位置选择 ROM，导入会复制到 `assests/roms/`，同时复制源 ROM 旁的 `.sav` / `.rtc`。
  同内容、同名 ROM 复用已有条目；不同内容的重名文件自动添加编号。
  现有 ROM 和存档不会被覆盖。无效、截断或不支持的 ROM 会显示错误。

可通过参数指定位置：

```powershell
out/build/release/PixelLink.exe --rom-dir "D:/Games/GameBoy"
out/build/release/PixelLink.exe --rom "D:/Games/GameBoy/example.gb"
```

`--rom` 打开游戏库之外的 ROM 时，会先复制到游戏库再启动，存档也写入游戏库中的副本旁。
`--rom-dir` 可指定其它游戏库目录，扫描、导入和外部 ROM 复制都会使用指定目录。

## 游戏操作与存档

| 操作 | 按键 |
| --- | --- |
| 方向 | 方向键 |
| A / B | Z / X |
| Start / Select | Enter / Backspace |
| 暂停 / 继续 | 空格 |
| 静音 / 开启声音 | M |
| 保存 | F5，或“保存”按钮 |
| 保存并返回游戏库 | Esc，或“保存并返回游戏库”按钮 |

窗口失去焦点时自动暂停并释放按键。暂停时关闭旧音频流，继续时重新创建，
避免积压声音。每次进入游戏都创建全新的模拟器，切换游戏不会沿用上一个游戏的 CPU、RAM 或音频状态。

支持电池存档的卡带会自动读取 ROM 旁同名 `.sav`，MBC3 RTC 使用 `.rtc`。
每 30 秒自动保存；返回游戏库、切换游戏和关闭应用前也会保存。
写入通过同目录临时文件完成后再替换，避免失败写入直接截断旧存档。
保存失败时显示错误，并保留当前游戏，阻止返回或关闭，便于修复路径权限后重试。

这是游戏的电池存档，不是任意时刻的完整模拟器快照。没有电池存档的 ROM 会显示说明。
目前模拟的是 DMG；兼容 DMG 的 `.gbc` 可运行，CGB 专用 ROM 会明确提示不支持。

## 构建与测试

初始化 SDL 和 spdlog 子模块，在 Visual Studio 的 **x64 Native Tools Command Prompt** 中构建：

```powershell
git submodule update --init --recursive
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release --target PixelLink GameBoyTest DesktopTest FrontendTest --parallel
ctest --test-dir out/build/release -R '^(GameBoyTest|DesktopTest)$' --output-on-failure
```

### 依赖版本管理

依赖通过 Git 子模块固定到主仓库记录的精确提交，不自动跟随上游分支：

| 依赖 | 固定提交 | 说明 |
| --- | --- | --- |
| SDL | `829a65d769d935c4852f8159e964312c0957260a` | 官方稳定发布版 [3.4.18](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18)，标签 `release-3.4.18` |
| spdlog | `79524ddd08a4ec981b7fea76afd08ee05f83755d` | v1.17.0 |

正常初始化或同步使用 `git submodule update --init --recursive --checkout`。
上游新增提交、执行 fetch 或更新主项目时，不会自动切换到 SDL 上游最新版本；
子模块仍以主项目记录的提交为准。不要在日常同步中使用 `git submodule update --remote`。

需要 SDL 的修复或新功能时，单独进行依赖升级，优先选择稳定发布标签；
重新构建 Debug / Release，运行 `GameBoyTest`、`DesktopTest` 和
`DesktopTest --real-devices`，再检查实际游戏画面和声音。
验收通过后，将新的子模块提交指针与本节版本记录一起提交到主仓库。

Android 工程中的 `.project`、`.classpath` 和 `.settings` 内的 Eclipse / Buildship
配置是本地 IDE 生成文件，不应提交为 SDL 源码修改。本机已在 SDL 子模块的
Git 本地排除配置中忽略这类文件；该配置不会修改上游 SDL，也不随主仓库传播。

本机中文 MSVC 的 `/showIncludes` 前缀被 CMake 自动识别成乱码，需要在配置时加：
`"-DPIXELLINK_MSVC_INCLUDE_PREFIX=注意: 包含文件:"`。
此选项仅覆盖 Ninja 的依赖识别前缀；其它语言或生成器保持默认。

- `GameBoyTest`：核心单元测试、40 个 Mooneye ROM 和 12 个 Blargg DMG APU ROM。
- `DesktopTest`：自动音频检查、ROM 导入、中文路径、输入、暂停、存档、失败恢复、
  两款本地游戏 PCM 和窗口截图。正常运行使用 dummy 视频、音频驱动，不弹出交互窗口。
- `DesktopTest --real-audio`：隐藏测试窗口，使用真实音频设备进行有时长上限的播放检查。
  它验证设备打开和样本提交，声音是否悦耳或完全准确仍需要实际听音确认。
- `DesktopTest --real-devices`：隐藏窗口，使用原生视频后端和真实音频设备验证绘制、呈现和播放。
- `FrontendTest`：保留原来的交互游戏测试，需要手动关闭窗口，标签为 `interactive`。

测试日志和截图在构建目录 `logs/`、`DesktopTest/`；测试生成的 ROM、存档在专用临时目录中清理，
不会修改游戏库中的存档。阶段进度见 `ProjectPlan.md`。

Visual Studio 多配置构建使用 `--config Release`（或 `Debug`），运行 CTest 时使用
`-C Release`（或 `-C Debug`），例如：

```powershell
cmake --build out/build/local-msvc --config Release --target PixelLink GameBoyTest DesktopTest FrontendTest --parallel
ctest --test-dir out/build/local-msvc -C Release -R '^(GameBoyTest|DesktopTest)$' --output-on-failure
```

### 测试结构与日志

测试入口位于 `Test/PixelLink/{GameBoy,Desktop,Frontend}/Main.cpp`，套件在
`include/PixelLink/Test/TestSuites.hpp` 声明。新增用例使用 `Test::run` 和 `CHECK`；
失败会输出诊断并使程序返回非零状态。硬件 ROM 清单由
`HardwareAccuracyTest.cpp` 和 `APUAccuracyTest.cpp` 显式维护，缺失 ROM 按失败处理。
ROM 来源、固定版本和 SHA-256 保存在 `assests/roms/` 对应子目录的清单中。

三个测试程序共享带时间戳的日志，记录每项用例的结果和耗时，构建目录的 `logs/`
合计保留最近 10 份测试运行日志。通过环境变量 `SPDLOG_LEVEL=debug` 可开启更详细的输出。
桌面测试的共享夹具位于 `Test/PixelLink/Desktop/TestUtils.cpp`，声明位于
`include/PixelLink/TestUtils/Desktop.hpp`；测试入口与日志策略由 `PixelLink::TestSupport`
管理，通用日志工具由 `PixelLink::Utils` 提供。

## DMG 准确性与阶段验收

当前目标是 DMG 模拟。CPU、定时器、中断、DMA、串口、映射器、RTC、声音、输入和电池存档
已有自动回归；声音和存档也已由用户实际确认。

PPU 使用逐 dot 的背景/窗口取数器、背景与精灵像素 FIFO，在像素输出时读取调色板，
不再于扫描线结束后整行重画。实现包含分阶段图块与低/高位平面读取、细滚动丢弃、
窗口取数重启、WY 触发锁存、OAM 搜索及精灵取数停顿、透明与优先级混合、LCD 关闭清空。
回归覆盖绘制中途的调色板、滚动、窗口开关和存储写入，以及逐次/批量时钟推进的一致性。
实现参考 [Pan Docs Pixel FIFO](https://github.com/gbdev/pandocs/blob/master/src/pixel_fifo.md)
和 [Rendering](https://github.com/gbdev/pandocs/blob/master/src/Rendering.md)。

准确性以所选的 40 个 Mooneye、12 个 Blargg DMG 音频 ROM 和核心回归为验收范围，
不等于通过完整上游测试集或覆盖所有硬件修订。CGB/SGB 专用行为、实际 boot ROM 启动、
OAM corruption 和未经独立硬件 ROM 覆盖的极端寄存器竞争/芯片差异仍属于后续扩展。
串口当前提供核心逐位传输能力，前端联机控制属于第二阶段。

2026-10-25 FIFO 回归：Debug / Release 的 `GameBoyTest` 均 **211/211** 通过，
包括 **17 项 FIFO 回归、40 个 Mooneye、12 个 Blargg 音频 ROM**；
两种配置的核心与桌面 CTest 均 **2/2** 通过，两款本地游戏截图已检查。
Release 的 `DesktopTest --real-devices` 也通过 Windows 视频和 WASAPI 真实设备检查，
日志为 `out/fifo-device-tests.log`。
记录位于 `out/fifo-release-tests.log`、`out/fifo-debug-tests.log`，
JUnit 结果为构建目录中的 `fifo-release-results.xml`、`fifo-debug-results.xml`。
