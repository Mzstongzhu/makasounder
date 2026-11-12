\# MAKASOUNDER



MAKASOUNDER 是一个纯 Win32 的轻量音乐播放器，面向 Windows 7 / 10 / 11（32 位优先）。没有 Electron，没有 WebView，没有持续自绘刷新；主界面、播放列表、歌词、设置和歌词制作器全部使用系统原生控件。



项目包含两个版本：



\- \*\*MAKASOUNDER（离线版）\*\*：纯本地播放器，不联网、不上传任何数据。

\- \*\*MAKASOUNDER-OL（在线版）\*\*：在离线版全部功能之上，增加“连接社区服务器 → 搜索歌曲 → 下载并加入播放列表”的能力。



两个版本可以同时保留、分别使用，互不影响。



\## 两个版本怎么选



| | 离线版 | 在线版 |

|---|---|---|

| 本地播放 | ✅ | ✅ |

| 悬浮窗 / 歌词制作器 / 文件关联 | ✅ | ✅ |

| 在线搜索与下载 | — | ✅ |

| 需要服务器 | 否 | 可选（不配置时等价于离线播放器） |

| 网络访问 | 无 | 仅访问你配置的服务器 |

| 额外链接库 | — | `ws2\_32` |



只需要本地播放，用离线版；有自己的曲库服务器、或想和朋友共享一个曲库，用在线版。



\## 功能



\### 共有功能



\- 播放 MP3 / WAV / WMA；播放失败时自动走 \*\*BASS → MCI → 清理 ID3 标签的临时副本 → DirectShow\*\* 回退链。可选 BASS 后端扩展 FLAC / AAC / OPUS / APE 等格式。

\- 专门处理“封面/标签损坏导致 MCI 或 DirectShow 打不开 MP3”的情况：自动生成只含音频帧的临时副本，切歌或退出时删除。

\- 启动时自动递归扫描 \*\*exe 所在目录\*\*的音频并导入播放列表；新文件超过 200 首会先询问是否继续。

\- 标准 Win32 主窗口：工具栏、播放列表（虚拟列表）、歌词框、进度条、音量、状态栏。

\- 内嵌封面读取与缓存：ID3v2 `APIC`、FLAC / Ogg Vorbis `PICTURE`、MP4 `covr`，无封面时使用内置占位图。

\- 歌词回退链：内嵌同步歌词（SYLT 或带时间戳的 USLT）→ 同名 `.lrc` → 标题标签对应的 `.lrc` → 内嵌纯文本。

\- 桌面悬浮窗：置顶、无标题栏、简单原生按钮；拖到屏幕边缘松手后通过约 240ms 缓动动画缩成圆形封面球。鼠标移上去自动展开，移开自动缩回；拖动球离开边缘才会保持展开。动画只在状态切换时运行，不常驻刷新。

\- MP3 文件关联（只修改 `HKCU`），支持双击打开主界面或 `--float` 直达悬浮窗。

\- 原生设置对话框：贴边缩略开关、双击 MP3 的行为、注册 / 移除关联。

\- 原生“歌词制作器”：第一遍边听边逐行输入歌词（回车添加），第二遍回到开头逐行打轴（F8 或回车），支持拖动顶部进度条、删除、撤销、双击试听、导入和保存 LRC。

\- 支持 Windows 标准媒体键：笔记本 Fn 播放/暂停、上一首、下一首、停止；程序窗口或歌词制作器在前台时生效。

\- 多实例启动，不强制单实例。



\### 在线版新增



\- 「工具 → 在线搜索歌曲...」：连接社区服务器搜索、下载并加入播放列表。

\- 「工具 → 服务器设置...」：配置服务器地址、测试连接、注册/登录、公开模式开关。

\- 下载的歌曲带歌词/封面时，会随文件一起缓存到本地；离线状态下仍可播放已缓存的歌曲。

\- 下载缓存位于 `%APPDATA%\\MAKASOUNDER\\cache\\`，文件名形如 `歌曲名\_曲目ID.mp3`（保留源文件扩展名），重复下载会直接复用缓存。



\## 为什么没有用 TagLib / 没有绑定 BASS 开发库



原始方案中的 BASS + TagLib 需要额外 SDK、库文件和 32 位编译配置，不适合直接丢进 Dev-C++ 的 MinGW 环境。本实现做了两个兼容处理：



1\. \*\*元数据\*\*：内置轻量解析器，直接读取 ID3v2.3/2.4（含 APIC / USLT / SYLT / ID3v1）、FLAC、Ogg Vorbis、Opus、MP4/M4A、WAV、APEv2。没有 TagLib 静态库，也没有额外 DLL。

2\. \*\*音频\*\*：优先动态加载 exe 同目录的 `bass.dll`（存在时自动启用 BASS，并尝试加载 `bass\*.dll` 插件）；没有 BASS 时自动回退到 Windows 自带 MCI，再失败时尝试去掉 ID3 标签的临时副本，最后调用 DirectShow。这样普通 MP3 和大多数带封面/特殊 ID3 标签的 MP3 都能开箱即用；想要更多格式时只需把 BASS 官方 DLL 放进程序目录，不需要重新编译。



这也意味着：不发售、不分发 BASS DLL；如果使用 BASS，请自行遵守 un4seen 的授权条款。



\## 构建



两个版本都使用 Dev-C++ 自带的 MinGW（GCC 9.2.0，位于 `C:\\Program Files (x86)\\Dev-Cpp\\MinGW32\\bin`），C++17，Unicode，静态链接 libgcc / libstdc++。



\### 方法一：Windows 批处理（推荐）



离线版：



```bat

cd MAKASOUNDER

build.bat

```



生成 `build\\MAKASOUNDER.exe`。



在线版：



```bat

cd MAKASOUNDER-OL

build.bat

```



生成 `build\\MAKASOUNDER-OL.exe`。



如果 Dev-C++ 安装在其他目录，修改 `build.bat` 里的 `MINGW` 变量即可。构建脚本会自动使用 8.3 短路径，避免 `C:\\Program Files` 中的空格导致 `windres` 预处理失败。



\### 方法二：Dev-C++ 工程



\- 离线版：用 Dev-C++ 打开 `MAKASOUNDER.dev`，直接编译。

\- 在线版：用 Dev-C++ 打开 `MAKASOUNDER-OL.dev`，直接编译（额外链接 `ws2\_32`）。



工程已经包含：



\- C++17 编译选项

\- Unicode

\- 静态链接 libgcc / libstdc++

\- 链接 `comctl32`, `gdiplus`, `shlwapi`, `winmm`, `ole32`, `shell32`, `comdlg32` 等系统库（在线版另加 `ws2\_32`）

\- 资源文件 `MAKASOUNDER.rc`（图标、占位封面、清单、对话框、快捷键；在线版另含服务器设置、在线搜索对话框）



\### 方法三：Makefile



离线版在已经配置好 `g++` / `windres` 的 MinGW 命令行环境中：



```bat

mingw32-make

```



在线版使用 Dev-C++ 工程或 `build.bat`。



> 在线版已在当前环境验证：15 个源文件 + 资源全部编译通过，链接产物约 1.5 MB；`dist\\MAKASOUNDER-OL.exe` 为可直接运行的构建副本。



\## 使用



\### 基本操作



\- 点击“添加音乐”或“添加文件夹”，也可以把文件 / 文件夹直接拖进主窗口。

\- 启动时会自动扫描程序所在目录，把音频加入播放列表。目录较大时也可以在“工具 → 扫描软件目录音频”手动重新扫描。

&#x20; - 扫描范围是 exe 所在目录及其子目录，只追加尚未在列表中的文件。

&#x20; - 一次发现超过 200 首新文件会先弹窗确认；选择“否”则本次不导入。

\- 双击播放列表中的歌曲开始播放；回车播放选中项，Delete 删除选中项。

\- 快捷键：

&#x20; - `Space` 播放 / 暂停

&#x20; - `Ctrl+O` 添加文件

&#x20; - `Ctrl+Left` / `Ctrl+Right` 上一首 / 下一首

&#x20; - `Ctrl+Up` / `Ctrl+Down` 音量

\- “悬浮窗”按钮：隐藏主界面并显示置顶小窗。

\- 设置：“工具 → 设置”。文件关联只写入当前用户。



\### 悬浮窗



\- 拖动封面 / 文字区域移动

\- 拖到屏幕边缘松手，约 240ms 缓动动画收成圆形封面球

\- 鼠标指向球会自动展开，鼠标移开自动缩回

\- 单击球也会临时展开；只要鼠标离开，仍然会缩回，不会永久停留在展开态

\- 拖动球移动；把球拖离边缘后，窗口才会作为普通展开窗口保留

\- 右键菜单：返回主界面、展开 / 收起、贴边缩略开关、重置位置、退出



\### 歌词制作器



1\. 打开歌曲，菜单“工具 → 歌词制作器”。

2\. 第一遍：点击“播放”，在右侧输入框里边听边输入歌词，按回车或“添加行”；一行一行加入左侧列表。

3\. 第二遍：点击“从头开始”，播放回到 0:00，并自动选中第一行未打轴的歌词。顶部进度条可以直接拖动定位。

4\. 听到某一行时按 F8（或点击“打轴”/回车），把当前时间写进选中行，然后自动跳到下一行。

5\. 双击已打轴的行可以跳转试听；撤销、删除行用于修正。

6\. 点击“保存 LRC”，歌词写到歌曲同目录的同名 `.lrc`。



\## 在线版：服务器与在线搜索



\### 配置服务器



菜单「工具 → 服务器设置...」：



1\. 填入服务器地址，例如 `http://192.168.1.100:8080`（当前客户端只支持 HTTP）。

2\. 点「测试连接」，显示“连接成功”即网络可达。

3\. 公开服务器可直接使用；私有服务器需要先「注册」或「登录」。

4\. 点「确定」后地址、用户名、登录 token 会写入 `%APPDATA%\\MAKASOUNDER\\settings.ini`，下次启动自动连接。



\### 在线搜索



菜单「工具 → 在线搜索歌曲...」：



1\. 输入关键词后点「搜索」（或回车）。

2\. 双击结果，或选中后点「播放选中」：下载并立即播放。

3\. 点「添加选中」：下载并追加到播放列表。

4\. 点「下载选中」：只下载到缓存，不加入播放列表。



\### 离线使用



在线版本身也是完整的离线播放器：不配置服务器时，所有本地功能照常可用；只是无法搜索在线曲库。



\## 服务端



服务端位于在线版目录的 `server/`，纯 Python 标准库实现，\*\*Windows 与 Linux 均可直接运行\*\*：



```bash

\# Windows

python server.py --port 8080 --music ./music



\# Linux

python3 server.py --port 8080 --music ./music

```



把音频放进 `music/` 目录即可被搜索；同名 `.lrc` 会被当作歌词返回，内嵌 `APIC` 封面可被下载。

支持 `--private` 私有模式（需登录），默认公开模式。



完整的部署说明、参数、API 和 systemd / Windows 后台运行方式见 \[server/README.md](server/README.md)。



\## 命令行



```text

MAKASOUNDER.exe "C:\\Music\\song.mp3"

MAKASOUNDER.exe --float "C:\\Music\\song.mp3"

MAKASOUNDER.exe --float

```



在线版把 `MAKASOUNDER.exe` 换成 `MAKASOUNDER-OL.exe`，参数相同。



\- 普通启动：显示主界面并播放文件。

\- `--float`：不创建主界面，直接显示悬浮窗并播放。

\- 文件关联注册的默认命令\*\*不带\*\* `--float`；设置里可以选择“双击 MP3 时直达悬浮窗”。



\## 目录



两个版本目录可以并排，也可以把在线版放在离线版目录下；按实际布局调整即可。



```text

MAKASOUNDER/                 离线版

├─ src/                      C++ 源码

├─ assets/                   图标、占位封面

├─ build/                    构建输出

├─ MAKASOUNDER.rc            资源

├─ app.manifest              Windows 7–11 兼容清单

├─ build.bat                 MinGW 构建脚本

├─ MAKASOUNDER.dev           Dev-C++ 工程

├─ Makefile

└─ README.md



MAKASOUNDER-OL/              在线版

├─ src/                      C++ 源码（含 http\_client / json / online）

├─ assets/                   图标、占位封面

├─ server/                   社区服务器（Python，Windows/Linux）

│  ├─ server.py

│  └─ README.md

├─ build/                    构建输出

├─ dist/                     可直接运行的副本

├─ MAKASOUNDER.rc            资源（含服务器设置、在线搜索对话框）

├─ app.manifest

├─ build.bat                 MinGW 构建脚本

├─ MAKASOUNDER-OL.dev        Dev-C++ 工程

└─ README.md

```



\## 配置文件



`%APPDATA%\\MAKASOUNDER\\settings.ini`（UTF-8）：



\- 音量

\- 主窗口位置 / 大小

\- 悬浮窗位置、是否小窗、贴边开关

\- 双击 MP3 的偏好

\- 上次打开目录

\- （在线版）服务器地址、用户名、登录 token



下载缓存（在线版）：`%APPDATA%\\MAKASOUNDER\\cache\\`



\## 已知限制



\- 无 BASS 时，FLAC / AAC / OPUS / APE 等格式可能只有标签和封面，不能播放；请放入 32 位 BASS 官方 DLL（`bass.dll`）和对应插件（如 `bassflac.dll`、`bass\_aac.dll`、`bassopus.dll`、`basswma.dll`、`bass\_ape.dll`）。

\- MCI 在部分 Windows 版本上音量命令无效，音量以系统混音器为准。

\- 临时无标签副本放在 `%TEMP%\\MAKASOUNDER`，在切歌或退出时删除；如果程序被强制结束，残留文件可以直接删除，不影响原文件。

\- DirectShow 回退依赖系统已安装的解码器；极少数损坏文件仍可能无法播放。

\- Windows 10/11 的默认应用保护可能覆盖 `HKCU\\Software\\Classes\\.mp3`；如设置后仍被其他播放器打开，请到“设置 → 应用 → 默认应用”中选择 MAKASOUNDER。

\- 歌词制作器是“第一遍录词、第二遍打轴”的两遍式轻量版本，不含波形图；已打轴的行可以双击跳转试听。

\- 不支持全局媒体快捷键、托盘图标、在线歌词、封面联网获取。

\- 笔记本 Fn 媒体键通过 Windows 标准媒体键消息处理，程序窗口或制作器在前台时生效；不抢占其他播放器的全局热键。

\- 悬浮球贴边时采用“悬停临时展开、移开自动缩回”的交互；若要让悬浮窗长期展开，把球拖离屏幕边缘即可。

\- 为了兼容旧机器和高 DPI 显示，程序不声明 DPI 感知，由 Windows 统一缩放；在高 DPI 下会有轻微系统级模糊，但窗口不会被缩小或截断。

\- 编译使用 C++17（Dev-C++ 9.2.0 对 C++20 支持有限），代码不依赖 C++20 特性。



在线版额外限制：



\- 客户端仅支持 Windows（Win32 原生控件）；服务器支持 Windows / Linux / macOS。

\- 客户端当前只支持 HTTP，不支持 HTTPS；公网部署建议放在反向代理之后。

\- 公开模式下任何能访问服务器的人都可以搜索和下载，请按需使用 `--private`。

\- 服务端密码以无盐 SHA-256 保存、token 仅存内存（重启服务器后需重新登录），仅适合小规模 / 自用部署。

\- 登录 token 会保存在本机 `settings.ini` 中，请注意本机账户安全。



\## 许可证



Copyright © 2026 MakaZS-TonZhoo



This work is free. You can redistribute it and/or modify it under the terms of the Do What The Fuck You Want To Public License, Version 2, as published by Sam Hocevar. See the [LICENSE](LICENSE) file for more details.

