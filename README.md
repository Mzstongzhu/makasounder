

# MAKASOUNDER

> **冷知识**：你可以直接访问 [GitHub 仓库](https://github.com/Mzstongzhu/makasounder) 看代码、提 Issue，或者单纯点个 Star 让作者高兴一下。

> [*官方网站*](https://sounder.makazs.xyz/)：盘点那些**最不稳定也最没用的官网**

>[*我去你怎么知道我们开了个官方服务器*](https://music.makazs.xyz/)：盘点那些**储存最小最依赖cloudflare按需付费版的服务器**~~（不再依赖免费版算长大吗）~~
>    不过这里要说一下 官服的架构与加密方式与开源版的有所不同哦

> 我的软件只为了最佳的播放体验而生 如果你觉得当前版本对你而言太卡了 不妨试试其他版本 或者二开个新版本 关于许可证问题 页面底部会强调 不如现在就让我们进入正题吧

你说的对，但是MAKASOUNDER 是一个纯 Win32 的轻量音乐播放器，面向 Windows 7 / 10 / 11（32 位优先）。没有 Electron，没有 WebView，没有持续自绘刷新；主界面、播放列表、歌词、设置和歌词制作器全部使用系统原生控件。
（我不得不说 我上传的时间太晚了我已经忘了我改了什么 所以以下内容是一个与本项目无关的AI在阅读旧文档和RC文件后生成的 对不起 但是还是建议你自己去用用 毕竟都读到这了嘛 我会自己做插件的 插件的开发绝对绝对不会有任何AI插手 当然啦 你们也可以私下开发插件 总之额开源万岁我没话说了）
项目包含两个版本，可以直接点击下载其EXE二进制文件，或者点此下载其源码[ZIP包](https://github.com/Mzstongzhu/makasounder/archive/refs/tags/1.0.0.1.zip)或[tar包](https://github.com/Mzstongzhu/makasounder/archive/refs/tags/1.0.0.1.tar.gz)：

- [**MAKASOUNDER（离线版）**](https://github.com/Mzstongzhu/makasounder/releases/download/1.0.0.1/MAKASOUNDER.exe)：纯本地播放器，不联网、不上传任何数据。
- [**MAKASOUNDER-OL（在线版）**](https://github.com/Mzstongzhu/makasounder/releases/download/1.0.0.1/MAKASOUNDER-OL.exe)：在离线版全部功能之上，增加连接社区服务器、搜索歌曲、流式播放、下载、上传审核等能力。

两个版本可以同时保留、分别使用，互不影响。

## 两个版本怎么选

| 功能 | 离线版 | 在线版 |
| --- | --- | --- |
| 本地播放 | ✅ | ✅ |
| 悬浮窗 / 歌词制作器 / 文件关联 | ✅ | ✅ |
| 在线搜索 / 流式播放 / 下载 | — | ✅ |
| 上传音乐并提交审核 | — | ✅ |
| 需要服务器 | 否 | 可选（不配置时等价于离线播放器） |
| 网络访问 | 无 | 仅访问你配置的服务器 |
| 额外链接库 | — | `ws2_32` |

只需要本地播放，用离线版；有自己的曲库服务器、想和朋友共享曲库，或者想往曲库里传歌，用在线版。

## 功能

### 两个版本都有

- 播放 MP3 / WAV / WMA；播放失败时自动走 **BASS → MCI → 清理 ID3 标签的临时副本 → DirectShow** 回退链。可选 BASS 后端扩展 FLAC / AAC / OPUS / APE 等格式。
- 专门处理“封面/标签损坏导致 MCI 或 DirectShow 打不开 MP3”的情况：自动生成只含音频帧的临时副本，切歌或退出时删除。
- 启动时自动递归扫描 **exe 所在目录** 的音频并导入播放列表；新文件超过 200 首会先询问是否继续。
- 标准 Win32 主窗口：工具栏、播放列表（虚拟列表）、歌词框、进度条、音量、状态栏。
- 内嵌封面读取与缓存：ID3v2 `APIC`、FLAC / Ogg Vorbis `PICTURE`、MP4 `covr`，无封面时使用内置占位图。
- 歌词回退链：内嵌同步歌词（SYLT 或带时间戳的 USLT）→ 同名 `.lrc` → 标题标签对应的 `.lrc` → 内嵌纯文本。
- 桌面悬浮窗：置顶、无标题栏、简单原生按钮；拖到屏幕边缘松手后通过约 240ms 缓动动画缩成圆形封面球。鼠标移上去自动展开，移开自动缩回；拖动球离开边缘才会保持展开。动画只在状态切换时运行，不常驻刷新。
- MP3 文件关联（只修改 `HKCU`），支持双击打开主界面或 `--float` 直达悬浮窗。
- 原生设置对话框：贴边缩略开关、双击 MP3 的行为、注册 / 移除关联。
- 原生“歌词制作器”：第一遍边听边逐行输入歌词（回车添加），第二遍回到开头逐行打轴（F8 或回车），支持拖动顶部进度条、删除、撤销、双击试听、导入和保存 LRC。
- 支持 Windows 标准媒体键：笔记本 Fn 播放/暂停、上一首、下一首、停止；程序窗口或歌词制作器在前台时生效。
- 多实例启动，不强制单实例。

### 在线版额外有的

- **「工具 → 在线搜索歌曲...」**（`Ctrl+F`）：搜索、流式播放、下载并加入播放列表。
  - 「搜索」：按关键词搜。
  - 「流式播放」：选中结果直接开播，先听为快。缓存细节以源码为准。
  - 「下载并加入列表」：下载完直接追加到播放列表。
- **「工具 → 上传音乐...」**（`Ctrl+U`）：选一个音频文件，提交审核。不是传完就进曲库，要服主审核。
- **「工具 → 服务器设置...」**：配置服务器地址、用户名、密码；支持测试连接、登录、注册、退出登录。
  - 公开服务器不用登录也能搜、能播。
  - 私有服务器就老老实实登录或注册。
- 下载的歌曲带歌词/封面时，会随文件一起缓存到本地；离线状态下仍可播放已缓存的歌曲。
- 下载缓存位于 `%APPDATA%\MAKASOUNDER\cache\`，文件名形如 `歌曲名_曲目ID.mp3`（保留源文件扩展名），重复下载会直接复用缓存。具体以源码为准。

## 快捷键

两个版本都有的：

| 快捷键 | 作用 |
| --- | --- |
| `Space` | 播放 / 暂停 |
| `Ctrl+O` | 添加文件 |
| `Ctrl+Left` / `Ctrl+Right` | 上一首 / 下一首 |
| `Ctrl+Up` / `Ctrl+Down` | 音量 |
| 媒体键 | 播放/暂停、上一首、下一首、停止 |

在线版额外：

| 快捷键 | 作用 |
| --- | --- |
| `Ctrl+F` | 在线搜索 |
| `Ctrl+U` | 上传音乐 |

## 为什么没有用 TagLib / 没有绑定 BASS 开发库

原始方案里的 BASS + TagLib 要额外 SDK、库文件和 32 位编译配置，往 Dev-C++ 的 MinGW 里一塞就一堆事。所以这里做了两个兼容处理：

1. **元数据**：内置轻量解析器，直接读 ID3v2.3/2.4（含 APIC / USLT / SYLT / ID3v1）、FLAC、Ogg Vorbis、Opus、MP4/M4A、WAV、APEv2。没有 TagLib 静态库，也没有额外 DLL。
2. **音频**：优先动态加载 exe 同目录的 `bass.dll`；有 BASS 就自动启用，并尝试加载 `bass\*.dll` 插件。没有 BASS 就回退到 Windows 自带 MCI，再失败就试去掉 ID3 标签的临时副本，最后 DirectShow。普通 MP3 和大多数带封面/特殊标签的 MP3 开箱即用；想要更多格式，把 BASS 官方 DLL 丢进程序目录即可，不用重新编译。

所以：不发售、不分发 BASS DLL；用 BASS 就自己遵守 un4seen 的授权条款。

## 构建

两个版本都使用 Dev-C++ 自带的 MinGW（GCC 9.2.0，位于 `C:\Program Files (x86)\Dev-Cpp\MinGW32\bin`），C++17，Unicode，静态链接 libgcc / libstdc++。

### 方法一：批处理（推荐）

离线版：

```bat
cd MAKASOUNDER
build.bat
```

生成 `build\MAKASOUNDER.exe`。

在线版：

```bat
cd MAKASOUNDER-OL
build.bat
```

生成 `build\MAKASOUNDER-OL.exe`。

Dev-C++ 装在别处就改 `build.bat` 里的 `MINGW` 变量。脚本会自动用 8.3 短路径，免得 `C:\Program Files` 里的空格把 `windres` 预处理搞崩。

### 方法二：Dev-C++ 工程

- 离线版：打开 `MAKASOUNDER.dev`，编译。
- 在线版：打开 `MAKASOUNDER-OL.dev`，编译（额外链接 `ws2_32`）。

工程里已经配好：

- C++17
- Unicode
- 静态链接 libgcc / libstdc++
- 链接 `comctl32`, `gdiplus`, `shlwapi`, `winmm`, `ole32`, `shell32`, `comdlg32` 等系统库（在线版另加 `ws2_32`）
- 资源文件 `MAKASOUNDER.rc`：图标、占位封面、清单、设置、歌词制作器、快捷键；在线版另含服务器设置、在线搜索、上传音乐对话框

### 方法三：Makefile

离线版在配好 `g++` / `windres` 的 MinGW 命令行里：

```bat
mingw32-make
```

在线版用 Dev-C++ 工程或 `build.bat`。

## 使用

### 基本操作

- 点“添加音乐”或“添加文件夹”，也可以把文件 / 文件夹直接拖进主窗口。
- 启动时会自动扫描程序所在目录，把音频加入播放列表。目录太大也可以在“工具 → 扫描软件目录音频”手动重扫。
  - 扫描范围是 exe 所在目录及其子目录，只追加还没在列表里的文件。
  - 一次发现超过 200 首新文件会先弹窗确认；选“否”就这次不导入。
- 双击播放列表里的歌开始播放；回车播放选中项，Delete 删除选中项。
- “悬浮窗”按钮：隐藏主界面，显示置顶小窗。
- 设置：“工具 → 设置”。文件关联只写当前用户。

### 悬浮窗

- 拖封面 / 文字区域移动。
- 拖到屏幕边缘松手，约 240ms 缓动收成圆形封面球。
- 鼠标指上去自动展开，移开自动缩回。
- 单击球也会临时展开；鼠标一离开还是缩回，不会赖着不走。
- 拖球移动；把球拖离边缘后，窗口才会作为普通展开窗口保留。
- 右键菜单：返回主界面、展开 / 收起、贴边缩略开关、重置位置、退出。

### 歌词制作器

1. 打开歌曲，菜单“工具 → 歌词制作器”。
2. 第一遍：点“播放”，在右侧输入框里边听边输入歌词，按回车或“添加行”；一行一行加到左侧列表。
3. 第二遍：点“从头开始”，播放回到 0:00，并自动选中第一行还没打轴的歌词。顶部进度条可以直接拖动定位。
4. 听到某一行时按 F8（或点“打轴”/回车），把当前时间写进选中行，然后自动跳到下一行。
5. 双击已打轴的行可以跳转试听；撤销、删除行用来修正。
6. 点“保存 LRC”，歌词写到歌曲同目录的同名 `.lrc`。

## 在线版：服务器、搜索与上传

### 配置服务器

菜单「工具 → 服务器设置...」：

1. 填服务器地址，例如 `http://192.168.1.100:8080`（客户端目前只支持 HTTP）。
2. 点「测试连接」，显示连接成功就是网络通了。
3. 公开服务器可以直接搜索和播放；私有服务器填用户名、密码，然后「登录」或「注册」。
4. 想换账号就点「退出登录」。
5. 点「确定」后，地址、用户名、登录 token 会写入 `%APPDATA%\MAKASOUNDER\settings.ini`，下次启动自动连接。

### 在线搜索

菜单「工具 → 在线搜索歌曲...」（`Ctrl+F`）：

1. 输入关键词，点「搜索」或回车。
2. 选中结果点「流式播放」：直接开播，先听为快。
3. 选中结果点「下载并加入列表」：下载完追加到播放列表。

旧文档里的“播放选中 / 添加选中 / 下载选中”已经过时，现在界面上就是上面这几个按钮。

### 上传音乐

菜单「工具 → 上传音乐...」（`Ctrl+U`）：

1. 点「浏览」选一个音频文件。
2. 点「提交审核」。
3. 状态栏会显示提交结果。提交后不是立刻进曲库，要服主审核。

审核怎么审、文件存哪、接口长什么样，看服务端说明。

### 离线使用

在线版本身也是完整的离线播放器：不配置服务器时，本地功能照常可用；只是搜不了在线曲库，也传不了歌。

## 服务端

服务端在在线版目录的 `server/`，纯 Python 标准库实现，Windows 和 Linux 都能直接跑：

```bash
# Windows
python server.py --port 8080 --music ./music

# Linux
python3 server.py --port 8080 --music ./music
```

把音频丢进 `music/` 目录就能被搜索；同名 `.lrc` 会被当作歌词返回，内嵌 `APIC` 封面可被下载。支持 `--private` 私有模式（需登录），默认公开模式。

完整部署说明、参数、API 和 systemd / Windows 后台运行方式见 [server/README.md](https://github.com/Mzstongzhu/makasounder/blob/main/MAKASOUNDER-OL/server/README.md)。

## 命令行

```text
MAKASOUNDER.exe "C:\Music\song.mp3"
MAKASOUNDER.exe --float "C:\Music\song.mp3"
MAKASOUNDER.exe --float
```

在线版把 `MAKASOUNDER.exe` 换成 `MAKASOUNDER-OL.exe`，参数一样。

- 普通启动：显示主界面并播放文件。
- `--float`：不创建主界面，直接显示悬浮窗并播放。
- 文件关联注册的默认命令**不带** `--float`；设置里可以选“双击 MP3 时直达悬浮窗”。

## 目录

两个版本目录可以并排，也可以把在线版放在离线版目录下；按实际布局调整。

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
├─ src/                      C++ 源码（含 http_client / json / online）
├─ assets/                   图标、占位封面
├─ server/                   社区服务器（Python，Windows/Linux）
│  ├─ server.py
│  └─ README.md
├─ build/                    构建输出
├─ dist/                     可直接运行的副本
├─ MAKASOUNDER.rc            资源（含服务器设置、在线搜索、上传音乐对话框）
├─ app.manifest
├─ build.bat                 MinGW 构建脚本
├─ MAKASOUNDER-OL.dev        Dev-C++ 工程
└─ README.md
```

## 配置文件

`%APPDATA%\MAKASOUNDER\settings.ini`（UTF-8）：

- 音量
- 主窗口位置 / 大小
- 悬浮窗位置、是否小窗、贴边开关
- 双击 MP3 的偏好
- 上次打开目录
- （在线版）服务器地址、用户名、登录 token

下载缓存（在线版）：`%APPDATA%\MAKASOUNDER\cache\`

## 已知限制

- 无 BASS 时，FLAC / AAC / OPUS / APE 等格式可能只有标签和封面，不能播放；请放入 32 位 BASS 官方 DLL（`bass.dll`）和对应插件（如 `bassflac.dll`、`bass_aac.dll`、`bassopus.dll`、`basswma.dll`、`bass_ape.dll`）。
- MCI 在部分 Windows 版本上音量命令无效，音量以系统混音器为准。
- 临时无标签副本放在 `%TEMP%\MAKASOUNDER`，切歌或退出时删除；程序被强制结束的话，残留文件可以直接删，不影响原文件。
- DirectShow 回退依赖系统已安装的解码器；极少数损坏文件仍可能无法播放。
- Windows 10/11 的默认应用保护可能覆盖 `HKCU\Software\Classes\.mp3`；设置后还被别的播放器打开，就去“设置 → 应用 → 默认应用”里手动选 MAKASOUNDER。
- 歌词制作器是“第一遍录词、第二遍打轴”的两遍式轻量版本，没有波形图；已打轴的行可以双击跳转试听。
- 不支持全局媒体快捷键、托盘图标、在线歌词、封面联网获取。
- 笔记本 Fn 媒体键通过 Windows 标准媒体键消息处理，程序窗口或制作器在前台时生效；不抢别的播放器全局热键。
- 悬浮球贴边时是“悬停临时展开、移开自动缩回”；想让悬浮窗长期展开，把球拖离屏幕边缘。
- 为了兼容旧机器和高 DPI，程序不声明 DPI 感知，由 Windows 统一缩放；高 DPI 下会有点系统级模糊，但窗口不会被缩小或截断。
- 编译用 C++17（Dev-C++ 9.2.0 对 C++20 支持有限），代码不依赖 C++20 特性。

在线版额外限制：

- 客户端只支持 Windows（Win32 原生控件）；服务器支持 Windows / Linux / macOS。
- 客户端目前只支持 HTTP，不支持 HTTPS；公网部署建议放反向代理后面。
- 公开模式下任何能访问服务器的人都能搜索和下载，按需用 `--private`。
- 上传音乐要服主审核，不是提交完立刻上架。
- 在线搜索界面目前只有「流式播放」和「下载并加入列表」，旧文档里的“播放选中 / 添加选中 / 下载选中”已过时。
- 服务器设置里没有“公开模式开关”；公开还是私有，由服务器启动参数决定。
- 服务端密码以无盐 SHA-256 保存、token 仅存内存（重启服务器后要重新登录），只适合小规模 / 自用部署。
- 登录 token 会保存在本机 `settings.ini` 里，注意本机账户安全。
- 在线版版本信息里的 `FileDescription` 还写着 offline audio player，历史遗留，不影响运行。

## 许可证

应用本体无需商用许可，二开全程遵循 WTFPL v2，宽松许可，允许任意用途。

Copyright © 2026 MakaZS-TonZhoo

This work is free. You can redistribute it and/or modify it under the terms of the Do What The Fuck You Want To Public License, Version 2, as published by Sam Hocevar. See the [LICENSE](https://github.com/Mzstongzhu/makasounder/blob/main/LICENSE) file for more details.

应用内音频解析播放使用 BASS 库，该音频库版权归 Un4seen Developments Ltd. 所有，免费使用完全免费；商用时按照版权方要求，需向 [un4seen](https://www.un4seen.com/) 购买许可证。
