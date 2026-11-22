# MAKASOUNDER 社区服务器

纯 Python 标准库实现，无第三方依赖，可在 **Windows / Linux / macOS** 上运行。
客户端（MAKASOUNDER-OL）通过 HTTP 连接本服务器进行搜索、下载、歌词与封面获取。

## 目录结构

```text
server/
├─ server.py       服务器主程序
├─ music/          曲库目录（自动递归扫描，首次运行自动创建）
├─ users.json      用户数据（注册后自动生成）
└─ README.md
```

## 快速开始

### Windows

```bat
cd server
python server.py --port 8080 --music ./music
```

### Linux

```bash
cd server
python3 server.py --port 8080 --music ./music
```

启动后输出：

```text
MAKASOUNDER 社区服务器
  音乐目录: /path/to/server/music
  端口: 8080
  公开模式: 是（无需登录）
  访问: http://127.0.0.1:8080/api/ping
```

把音频文件（MP3 / FLAC / WAV / M4A / AAC / OGG / WMA / APE）放进 `music/` 即可被搜索到。
歌词放在同名 `.lrc` 旁边会被自动识别；封面从 ID3 `APIC` 帧读取。

## 运行参数

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `--port` | 监听端口 | `8080` |
| `--music` | 曲库目录 | `./music` |
| `--private` | 私有模式：搜索/下载需要登录 | 公开模式 |

## 公开模式与私有模式

- **公开模式（默认）**：任何客户端都能搜索和下载，适合局域网/家庭使用。
- **私有模式（`--private`）**：客户端必须注册或登录后才能搜索和下载。

客户端在「工具 → 服务器设置」中填写服务器地址：

```text
http://192.168.1.100:8080
```

## API

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/api/ping` | 健康检查，返回曲库数量与是否公开 |
| GET | `/api/search?q=关键词` | 搜索歌曲，返回 JSON 数组 |
| GET | `/api/download/{id}` | 下载音频文件 |
| GET | `/api/stream/{id}` | 流式播放（当前客户端主要用下载） |
| GET | `/api/lyrics/{id}` | 获取同名 `.lrc` 歌词 |
| GET | `/api/artwork/{id}` | 获取内嵌封面 |
| POST | `/api/register` | 注册，body：`{"username":"...","password":"..."}` |
| POST | `/api/login` | 登录，body 同上 |

鉴权使用请求头：

```text
Authorization: Bearer <token>
```

## 后台运行

### Linux（systemd）

创建 `/etc/systemd/system/makasounder.service`：

```ini
[Unit]
Description=MAKASOUNDER community server
After=network.target

[Service]
WorkingDirectory=/opt/makasounder/server
ExecStart=/usr/bin/python3 /opt/makasounder/server/server.py --port 8080 --music /srv/music
Restart=on-failure
User=www-data

[Install]
WantedBy=multi-user.target
```

然后：

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now makasounder
```

### Windows（后台运行）

最简单的方式是用 `pythonw` 无窗口启动，或使用任务计划程序 / NSSM 注册为服务：

```bat
pythonw server.py --port 8080 --music D:\Music
```

## 常见问题

- **客户端提示“无法连接到服务器”**：检查服务器是否启动、防火墙是否放行端口、客户端地址是否写成 `http://IP:端口`。
- **搜索为空**：确认 `music/` 下有受支持的音频扩展名；服务器启动时扫描一次，新增文件后重启服务器即可刷新曲库。
- **中文乱码**：服务器按 UTF-8 输出 JSON；客户端内部使用 Unicode。
- **安全提示**：`users.json` 中的密码为 SHA-256 摘要但无盐值，token 保存在内存中，重启服务器后需要重新登录。公网部署请放在反向代理（HTTPS）之后，并自行加强认证与限流。
