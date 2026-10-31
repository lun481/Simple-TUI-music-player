# Terminal Music Player

一个使用 C++20 编写的 Windows 终端本地音乐播放器。

- TUI：FTXUI 7
- 音频：miniaudio 0.11（WASAPI / DirectSound / WinMM 后端）
- 构建：CMake + vcpkg manifest
- 平台：Windows 10/11（建议使用 Windows Terminal）

支持格式：`.mp3`、`.flac`、`.wav`、`.ogg`、`.oga`。  
不支持 `.m4a`、`.aac`、`.opus`（miniaudio 内置解码器不包含这些格式）。

---

## 1. 项目目录结构

```text
project2/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── README.md
├── .gitignore
├── music/
│   └── README.md              # 把音乐文件放在这里
└── src/
    ├── main.cpp               # 入口、参数解析、--list / --probe
    ├── audio/
    │   ├── AudioEngine.h
    │   └── AudioEngine.cpp    # miniaudio 封装（加载/播放/暂停/跳转/音量）
    ├── core/
    │   ├── Playlist.h
    │   └── Playlist.cpp       # 目录扫描、排序、上一首/下一首
    ├── ui/
    │   ├── PlayerApp.h
    │   └── PlayerApp.cpp      # FTXUI 界面与键盘交互
    └── util/
        ├── Format.h           # 时间格式化
        └── PathUtil.h         # UTF-8 路径转换
```

构建产物默认位于：

```text
build/bin/Release/music-player.exe
```

---

## 2. vcpkg.json

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "terminal-music-player",
  "version-string": "1.0.0",
  "description": "A Windows terminal music player built with FTXUI and miniaudio.",
  "dependencies": [
    "ftxui",
    "miniaudio"
  ]
}
```

`ftxui` 负责终端界面，`miniaudio` 是单头文件音频库（vcpkg 只安装
`miniaudio.h`，没有 CMake config，因此 CMake 中用 `find_path` 定位）。

---

## 3. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)

project(TerminalMusicPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

find_package(ftxui CONFIG REQUIRED)

find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)
message(STATUS "miniaudio header: ${MINIAUDIO_INCLUDE_DIR}")

add_executable(music-player
    src/main.cpp
    src/audio/AudioEngine.cpp
    src/core/Playlist.cpp
    src/ui/PlayerApp.cpp
)

target_include_directories(music-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(music-player PRIVATE
    ftxui::component
    ftxui::dom
    ftxui::screen
)

if(WIN32)
    target_link_libraries(music-player PRIVATE ole32 winmm dsound avrt)
endif()

set_target_properties(music-player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
```

---

## 4. 依赖库手动安装

### 4.1 推荐方式：让 CMake 自动安装

```powershell
cd F:\opencode\project2
cmake --preset msvc-x64
```

CMake 会读取 `vcpkg.json`，把依赖安装到：

```text
build/vcpkg_installed/x64-windows/
```

安装的包：

- `ftxui:x64-windows@7.0.3`
- `miniaudio:x64-windows@0.11.25`

### 4.2 手动安装（与 CMake 使用同一目录）

如果希望先手动安装依赖、再配置 CMake，使用和 CMake 相同的安装目录：

```powershell
cd F:\opencode\project2
New-Item -ItemType Directory -Force build | Out-Null

F:\vcpkg\vcpkg-master\vcpkg.exe install --triplet x64-windows `
  --x-install-root="$PWD\build\vcpkg_installed"
```

> 注意：如果在项目根目录直接执行 `vcpkg install --triplet x64-windows`，
> vcpkg 默认安装到 `.\vcpkg_installed`，而 CMake 默认使用
> `build\vcpkg_installed`，两者不是同一个目录。

### 4.3 安装到 vcpkg 全局目录（可选）

```powershell
F:\vcpkg\vcpkg-master\vcpkg.exe install ftxui:x64-windows miniaudio:x64-windows
```

这个命令使用 classic 模式，把包安装到 `F:\vcpkg\vcpkg-master\installed\`。
本项目使用 manifest 模式，默认不会读取全局目录；保留此命令是为了方便其他
classic 项目复用同一套依赖。

### 4.4 没有网络加速器时

vcpkg 需要下载两个源码包。下载失败时，可以手动下载并放到
`F:\vcpkg\vcpkg-master\downloads\`：

| 文件 | 下载地址 |
| --- | --- |
| `ArthurSonzogni-FTXUI-v7.0.3.tar.gz` | https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v7.0.3.tar.gz |
| `mackron-miniaudio-0.11.25.tar.gz` | https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz |

然后重新执行上面的 `vcpkg install`。vcpkg 会校验 SHA512；如果文件名或内容
不匹配，会在报错信息中给出期望的文件名和哈希。

> 注意：`x64-windows` 是 **MSVC** triplet，不能用 MinGW 链接。本项目默认使用
> Visual Studio 2022 / MSVC 构建。如果你只装了 MinGW，请改用
> `x64-mingw-static` triplet 重新安装依赖，并在 CMake 中同步修改
> `VCPKG_TARGET_TRIPLET`。

---

## 5. 编译和运行

### 5.1 使用 CMakePresets（推荐）

```powershell
cd F:\opencode\project2

# 配置（会自动读取 vcpkg.json；依赖已安装时不会重复下载）
cmake --preset msvc-x64

# 编译 Release
cmake --build --preset release

# 运行（默认扫描 .\music）
.\build\bin\Release\music-player.exe
```

指定音乐目录：

```powershell
.\build\bin\Release\music-player.exe D:\Music
.\build\bin\Release\music-player.exe D:\Music -r
```

### 5.2 不使用 preset 的等价命令

```powershell
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=F:/vcpkg/vcpkg-master/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Release

.\build\bin\Release\music-player.exe .\music
```

### 5.3 调试与诊断

```powershell
# 只列出扫描到的曲目，不启动 TUI
.\build\bin\Release\music-player.exe .\music --list

# 初始化音频引擎并加载第一首，打印时长后退出
.\build\bin\Release\music-player.exe .\music --probe

# 实际执行播放/暂停/跳转/停止，并打印状态
.\build\bin\Release\music-player.exe .\music --selftest
```

---

## 6. 快捷键

| 按键 | 功能 |
| --- | --- |
| `Space` | 播放 / 暂停 |
| `Enter` | 播放当前选中曲目 |
| `↑` / `↓` | 在播放列表中移动 |
| `N` | 下一首 |
| `P` | 上一首（播放超过 3 秒时先回到本曲开头） |
| `S` | 停止并回到开头 |
| `←` / `→` | 后退 / 前进 5 秒 |
| `+` / `-` | 音量 +5% / -5% |
| `M` | 静音 / 恢复音量 |
| `R` | 重新扫描播放列表 |
| `Q` / `Esc` | 退出 |

---

## 7. 功能说明

- **播放 / 暂停 / 停止**：基于 `ma_sound_start`、`ma_sound_stop` 和 seek。
- **上一首 / 下一首**：列表循环；当前曲目播放超过 3 秒时，`P` 先回到本曲开头。
- **自动连播**：播放到结尾后自动切到下一首；单曲列表会循环播放。
- **进度显示**：200ms 刷新一次，显示当前时间 / 总时长和进度条。
- **进度跳转**：左右方向键每次 ±5 秒，自动限制在 `[0, duration]`。
- **音量调节**：0%–100%，支持静音/恢复；音量在切歌后保持。
- **播放列表**：扫描目录（可选递归），按文件名排序，支持滚动显示和选中高亮。
- **UTF-8 / 宽字符路径**：FTXUI 使用 UTF-8 字符串显示文件名；miniaudio 在
  Windows 下使用 `ma_sound_init_from_file_w` 宽字符接口，中文/日文文件名可正常
  显示和播放。
- **非阻塞 UI**：音频在 miniaudio 内部线程播放，UI 线程只做状态轮询和绘制；
  刷新线程通过 `PostEvent(Event::Custom)` 通知 TUI 重绘。

### 已知限制

- 不读取 ID3 / FLAC / Vorbis 标签，列表显示文件名（不含扩展名）。
- 不支持 `.m4a` / `.aac` / `.opus`。
- 没有搜索、随机播放、均衡器、歌词等功能。
- 终端字体需要支持 UTF-8；建议使用 Windows Terminal。

---

## 8. 常见问题

**Q: CMake 报 `Could not find a package configuration file provided by "ftxui"`**

依赖没有安装到 CMake 使用的 `build/vcpkg_installed`：

```powershell
cd F:\opencode\project2
New-Item -ItemType Directory -Force build | Out-Null
F:\vcpkg\vcpkg-master\vcpkg.exe install --triplet x64-windows `
  --x-install-root="$PWD\build\vcpkg_installed"
```

并确认配置时使用了：

```text
-DVCPKG_TARGET_TRIPLET=x64-windows
```

**Q: 链接时报 `unresolved external symbol __imp_CoInitializeEx` 等**

确认 `CMakeLists.txt` 中链接了 `ole32 winmm dsound avrt`（本项目已包含）。

**Q: 没有声音 / 初始化失败**

先运行 `--probe` 检查音频设备和解码器；确认系统默认播放设备可用。
如果是在远程桌面或无声卡的环境，`ma_engine_init` 可能失败。

**Q: 中文文件名乱码**

本项目使用 `wmain` + `pathToUtf8()`，并在启动时设置 UTF-8 代码页。
如果仍然乱码，请使用 Windows Terminal，并确认终端字体支持中文。
