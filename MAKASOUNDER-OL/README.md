# MAKASOUNDER-OL（在线版）

在线版 MAKASOUNDER：保留离线版的全部功能，并增加“连接服务器 → 搜索歌曲 → 添加进播放列表”的能力。
离线版（上级目录的 `MAKASOUNDER`）不受影响，两个版本可以同时保留、分别使用。

## 与离线版的区别

- 新增「工具 → 在线搜索歌曲...」：连接社区服务器搜索、下载并加入播放列表。
- 新增「工具 → 服务器设置...」：配置服务器地址、测试连接、注册/登录、公开模式开关。
- 下载的歌曲带歌词/封面时，会随文件一起缓存到本地；离线状态下仍可播放已缓存的歌曲。
- 离线版的全部功能保持不变：本地播放、播放列表、歌词、悬浮窗、歌词制作器、文件关联、媒体键、扫描软件目录等。

## 构建

### 方法一：Windows 批处理（推荐）

要求：Dev-C++ 自带的 MinGW 位于 `C:\Program Files (x86)\Dev-Cpp\MinGW32\bin`（GCC 9.2.0）。

```bat
build.bat
```

生成：`build\MAKASOUNDER-OL.exe`。

> 已经在当前环境验证：15 个源文件 + 资源全部编译通过，链接产物约 1.5 MB；`dist\MAKASOUNDER-OL.exe` 为可直接运行的构建副本。

### 方法二：Dev-C++ 工程

用 Dev-C++ 打开 `MAKASOUNDER-OL.dev` 直接编译。工程已包含 C++17、Unicode、静态链接 libgcc/libstdc++，并额外链接 `ws2_32`。

## 使用

### 1. 配置服务器

菜单「工具 → 服务器设置...」：

1. 填入服务器地址，例如 `http://192.168.1.100:8080`（当前客户端只支持 HTTP）。
2. 点「测试连接」，显示“连接成功”即网络可达。
3. 公开服务器可直接使用；私有服务器需要先「注册」或「登录」。
4. 点「确定」后地址、用户名、登录 token 会写入 `%APPDATA%\MAKASOUNDER\settings.ini`，下次启动自动连接。

### 2. 在线搜索

菜单「工具 → 在线搜索歌曲...」：

1. 输入关键词后点「搜索」（或回车）。
2. 双击结果，或选中后点「播放选中」：下载并立即播放。
3. 点「添加选中」：下载并追加到播放列表。
4. 点「下载选中」：只下载到缓存，不加入播放列表。

下载缓存位于 `%APPDATA%\MAKASOUNDER\cache\`，文件名形如 `歌曲名_曲目ID.mp3`（保留源文件扩展名），重复下载会直接复用缓存。

### 3. 离线使用

在线版本身也是完整的离线播放器：不配置服务器时，所有本地功能照常可用；只是无法搜索在线曲库。

## 服务端

服务端位于 `server/`，纯 Python 标准库实现，**Windows 与 Linux 均可直接运行**：

```bash
# Windows
python server.py --port 8080 --music ./music

# Linux
python3 server.py --port 8080 --music ./music
```

把音频放进 `music/` 目录即可被搜索；同名 `.lrc` 会被当作歌词返回，内嵌 `APIC` 封面可被下载。
支持 `--private` 私有模式（需登录），默认公开模式。

完整的部署说明、参数、API 和 systemd/Windows 后台运行方式见 [server/README.md](server/README.md)。

## 目录

```text
OL/
├─ src/                  C++ 源码（含 http_client / json / online）
├─ assets/               图标、占位封面
├─ server/               社区服务器（Python，Windows/Linux）
│  ├─ server.py
│  └─ README.md
├─ build/                构建输出
├─ dist/                 可直接运行的副本
├─ MAKASOUNDER.rc        资源（含服务器设置、在线搜索对话框）
├─ app.manifest
├─ build.bat             MinGW 构建脚本
├─ MAKASOUNDER-OL.dev    Dev-C++ 工程
└─ README.md
```

## 注意事项

- 客户端仅支持 Windows（Win32 原生控件）；服务器支持 Windows/Linux/macOS。
- 客户端当前只支持 HTTP，不支持 HTTPS；公网部署建议放在反向代理之后。
- 公开模式下任何能访问服务器的人都可以搜索和下载，请按需使用 `--private`。
- 服务端密码以无盐 SHA-256 保存、token 仅存内存（重启服务器后需重新登录），仅适合小规模/自用部署。
- 登录 token 会保存在本机 `settings.ini` 中，请注意本机账户安全。
- 兼容离线版所有功能：本地播放、歌词制作器、悬浮窗、文件关联等。
