# PixelLink

Windows / SDL3 的本地 Game Boy 模拟器，支持 ROM 游戏库、声音、键盘输入和电池存档。

## 运行

本次构建的程序：`out/build/release/PixelLink.exe`。将同目录的 `SDL3.dll`
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
cmake --build out/build/release --target PixelLink GameBoyTests DesktopTests FrontendTests --parallel
ctest --test-dir out/build/release -R '^(GameBoyTests|DesktopTests)$' --output-on-failure
```

本机中文 MSVC 的 `/showIncludes` 前缀被 CMake 自动识别成乱码，需要在配置时加：
`"-DPIXELLINK_MSVC_INCLUDE_PREFIX=注意: 包含文件:"`。
此选项仅覆盖 Ninja 的依赖识别前缀；其它语言或生成器保持默认。

- `GameBoyTests`：核心单元测试、40 个 Mooneye ROM 和 12 个 Blargg DMG APU ROM。
- `DesktopTests`：自动音频检查、ROM 导入、中文路径、输入、暂停、存档、失败恢复、
  两款本地游戏 PCM 和窗口截图。正常运行使用 dummy 视频、音频驱动，不弹出交互窗口。
- `DesktopTests --real-audio`：隐藏测试窗口，使用真实音频设备进行有时长上限的播放检查。
  它验证设备打开和样本提交，声音是否悦耳或完全准确仍需要实际听音确认。
- `DesktopTests --real-devices`：隐藏窗口，使用原生视频后端和真实音频设备验证绘制、呈现和播放。
- `FrontendTests`：保留原来的交互游戏测试，需要手动关闭窗口，标签为 `interactive`。

测试日志和截图在构建目录 `logs/`、`desktop-tests/`；测试生成的 ROM、存档在专用临时目录中清理，
不会修改游戏库中的存档。进度和准确性范围见 `ProjectPlan.md`、`HardwareAccuracy.md`。
