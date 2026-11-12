#!/usr/bin/env python3
"""
MAKASOUNDER 社区服务器示例（自部署）

特点：
- 纯 Python 标准库，无第三方依赖
- 扫描 music/ 目录下的音频文件作为曲库
- 自动读取 ID3v1/v2 标签、时长
- 提供搜索、下载、歌词、封面、登录/注册接口
- 用户信息保存在 users.json（明文，仅用于演示）

运行：
    python server.py [--port 8080] [--music ./music]

客户端在「设置」中填入 http://你的IP:8080 即可连接。
"""

import os
import sys
import json
import re
import hashlib
import time
import threading
import socket
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs, unquote

MUSIC_DIR = "./music"
PORT = 8080
USERS_FILE = "./users.json"
TOKENS = {}
USERS = {}

AUDIO_EXTS = {".mp3", ".flac", ".wav", ".m4a", ".aac", ".ogg", ".wma", ".ape"}


def load_users():
    global USERS
    if os.path.exists(USERS_FILE):
        try:
            with open(USERS_FILE, "r", encoding="utf-8") as f:
                USERS = json.load(f)
        except Exception:
            USERS = {}


def save_users():
    try:
        with open(USERS_FILE, "w", encoding="utf-8") as f:
            json.dump(USERS, f, ensure_ascii=False, indent=2)
    except Exception:
        pass


def scan_library():
    library = []
    if not os.path.isdir(MUSIC_DIR):
        return library
    for root, dirs, files in os.walk(MUSIC_DIR):
        for fname in files:
            ext = os.path.splitext(fname)[1].lower()
            if ext not in AUDIO_EXTS:
                continue
            fpath = os.path.join(root, fname)
            try:
                stat = os.stat(fpath)
            except Exception:
                continue
            track_id = hashlib.md5(
                (os.path.relpath(fpath, MUSIC_DIR)).encode("utf-8")
            ).hexdigest()[:12]
            title, artist, album, duration = read_tags(fpath)
            rel = os.path.relpath(fpath, MUSIC_DIR).replace("\\", "/")
            library.append(
                {
                    "id": track_id,
                    "title": title or os.path.splitext(fname)[0],
                    "artist": artist or "未知艺术家",
                    "album": album or "",
                    "duration": duration,
                    "path": fpath,
                    "rel": rel,
                    "ext": ext,
                    "size": stat.st_size,
                    "lyrics": os.path.exists(
                        os.path.splitext(fpath)[0] + ".lrc"
                    ),
                    "artwork": has_artwork(fpath),
                }
            )
    return library


def read_tags(fpath):
    title = artist = album = ""
    duration = "00:00"
    try:
        with open(fpath, "rb") as f:
            head = f.read(10)
            if head[:3] == b"ID3":
                size = (
                    (head[6] & 0x7F) << 21
                    | (head[7] & 0x7F) << 14
                    | (head[8] & 0x7F) << 7
                    | (head[9] & 0x7F)
                )
                tagdata = f.read(size)
                title, artist, album = parse_id3v2(tagdata)
    except Exception:
        pass
    try:
        duration = estimate_mp3_duration(fpath)
    except Exception:
        pass
    return title, artist, album, duration


def parse_id3v2(data):
    title = artist = album = ""
    pos = 0
    while pos + 10 <= len(data):
        fid = data[pos : pos + 4]
        if fid[0] == 0:
            break
        size = (data[pos + 4] << 24) | (data[pos + 5] << 16) | (data[pos + 6] << 8) | data[pos + 7]
        flags = (data[pos + 8] << 8) | data[pos + 9]
        pos += 10
        if pos + size > len(data):
            break
        frame = data[pos : pos + size]
        pos += size
        if flags & 0x0080:
            continue
        if fid == b"TIT2":
            title = decode_id3_text(frame)
        elif fid == b"TPE1":
            artist = decode_id3_text(frame)
        elif fid == b"TALB":
            album = decode_id3_text(frame)
    return title, artist, album


def decode_id3_text(frame):
    if not frame:
        return ""
    enc = frame[0]
    raw = frame[1:]
    try:
        if enc == 0:
            return raw.decode("latin-1")
        elif enc == 1:
            if raw[:2] == b"\xff\xfe":
                return raw[2:].decode("utf-16-le")
            return raw.decode("utf-16-le")
        elif enc == 2:
            return raw.decode("utf-16-be")
        elif enc == 3:
            return raw.decode("utf-8")
    except Exception:
        pass
    return raw.decode("latin-1", errors="ignore")


def estimate_mp3_duration(fpath):
    try:
        size = os.path.getsize(fpath)
        with open(fpath, "rb") as f:
            head = f.read(512)
        for i in range(len(head) - 4):
            if head[i] == 0xFF and (head[i + 1] & 0xE0) == 0xE0:
                bitrate_idx = (head[i + 2] >> 4) & 0x0F
                rates = [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0]
                if 1 <= bitrate_idx <= 14:
                    bitrate = rates[bitrate_idx] * 1000
                    secs = size * 8 / bitrate
                    mm = int(secs) // 60
                    ss = int(secs) % 60
                    return f"{mm:02d}:{ss:02d}"
    except Exception:
        pass
    return "00:00"


def has_artwork(fpath):
    try:
        with open(fpath, "rb") as f:
            head = f.read(10)
            if head[:3] == b"ID3":
                size = (
                    (head[6] & 0x7F) << 21
                    | (head[7] & 0x7F) << 14
                    | (head[8] & 0x7F) << 7
                    | (head[9] & 0x7F)
                )
                data = f.read(size)
                return b"APIC" in data
    except Exception:
        pass
    return False


def extract_artwork(fpath):
    try:
        with open(fpath, "rb") as f:
            head = f.read(10)
            if head[:3] != b"ID3":
                return None
            size = (
                (head[6] & 0x7F) << 21
                | (head[7] & 0x7F) << 14
                | (head[8] & 0x7F) << 7
                | (head[9] & 0x7F)
            )
            data = f.read(size)
            pos = 0
            while pos + 10 <= len(data):
                fid = data[pos : pos + 4]
                if fid[0] == 0:
                    break
                fsize = (data[pos + 4] << 24) | (data[pos + 5] << 16) | (data[pos + 6] << 8) | data[pos + 7]
                pos += 10
                if fid == b"APIC":
                    frame = data[pos : pos + fsize]
                    sep = frame.find(b"\x00", 1)
                    if sep > 0:
                        pic_type = frame[sep + 1]
                        sep2 = frame.find(b"\x00", sep + 2)
                        if sep2 > 0:
                            return frame[sep2 + 1 :]
                pos += fsize
    except Exception:
        pass
    return None


LIBRARY = []
LIBRARY_LOCK = threading.Lock()


def refresh_library():
    global LIBRARY
    lib = scan_library()
    with LIBRARY_LOCK:
        LIBRARY = lib
    print(f"[库] 扫描完成，共 {len(lib)} 首")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def _json(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _text(self, code, text):
        body = text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, fpath, mime="application/octet-stream"):
        try:
            size = os.path.getsize(fpath)
            self.send_response(200)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(size))
            self.end_headers()
            with open(fpath, "rb") as f:
                while True:
                    chunk = f.read(65536)
                    if not chunk:
                        break
                    self.wfile.write(chunk)
        except Exception:
            self.send_error(404)

    def _check_auth(self):
        auth = self.headers.get("Authorization", "")
        if auth.startswith("Bearer "):
            token = auth[7:]
            if token in TOKENS:
                return True
        return PUBLIC_MODE

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        if path == "/api/ping":
            return self._json(200, {"ok": True, "library": len(LIBRARY), "public": PUBLIC_MODE})

        if path == "/api/search":
            if not self._check_auth():
                return self._json(401, {"error": "未登录"})
            q = qs.get("q", [""])[0].lower()
            with LIBRARY_LOCK:
                results = []
                for t in LIBRARY:
                    if not q or q in t["title"].lower() or q in t["artist"].lower() or q in t["album"].lower():
                        results.append(
                            {
                                "id": t["id"],
                                "title": t["title"],
                                "artist": t["artist"],
                                "album": t["album"],
                                "duration": t["duration"],
                                "download": f"/api/download/{t['id']}",
                                "stream": f"/api/stream/{t['id']}",
                                "ext": t["ext"],
                                "lyrics": 1 if t["lyrics"] else 0,
                                "artwork": 1 if t["artwork"] else 0,
                            }
                        )
                        if len(results) >= 200:
                            break
            return self._json(200, results)

        if path.startswith("/api/download/") or path.startswith("/api/stream/"):
            if not self._check_auth():
                return self._json(401, {"error": "未登录"})
            tid = path.split("/")[-1]
            with LIBRARY_LOCK:
                track = next((t for t in LIBRARY if t["id"] == tid), None)
            if not track:
                return self._json(404, {"error": "曲目不存在"})
            mime = "audio/mpeg" if track["ext"] == ".mp3" else "application/octet-stream"
            return self._send_file(track["path"], mime)

        if path.startswith("/api/lyrics/"):
            if not self._check_auth():
                return self._json(401, {"error": "未登录"})
            tid = path.split("/")[-1]
            with LIBRARY_LOCK:
                track = next((t for t in LIBRARY if t["id"] == tid), None)
            if not track:
                return self._json(404, {"error": "曲目不存在"})
            lrc = os.path.splitext(track["path"])[0] + ".lrc"
            if os.path.exists(lrc):
                with open(lrc, "r", encoding="utf-8", errors="ignore") as f:
                    return self._text(200, f.read())
            return self._text(404, "")

        if path.startswith("/api/artwork/"):
            if not self._check_auth():
                return self._json(401, {"error": "未登录"})
            tid = path.split("/")[-1]
            with LIBRARY_LOCK:
                track = next((t for t in LIBRARY if t["id"] == tid), None)
            if not track:
                return self._json(404, {"error": "曲目不存在"})
            art = extract_artwork(track["path"])
            if art:
                self.send_response(200)
                self.send_header("Content-Type", "image/jpeg")
                self.send_header("Content-Length", str(len(art)))
                self.end_headers()
                self.wfile.write(art)
                return
            return self._json(404, {"error": "没有封面"})

        self.send_error(404)

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length) if length > 0 else b""
        try:
            data = json.loads(body) if body else {}
        except Exception:
            data = {}

        if path == "/api/register":
            user = data.get("username", "").strip()
            pwd = data.get("password", "")
            if not user or not pwd or len(user) < 2 or len(pwd) < 4:
                return self._json(400, {"error": "用户名至少 2 字符，密码至少 4 字符"})
            if user in USERS:
                return self._json(409, {"error": "用户名已存在"})
            USERS[user] = hashlib.sha256(pwd.encode()).hexdigest()
            save_users()
            token = user + "_" + hashlib.md5(str(time.time()).encode()).hexdigest()
            TOKENS[token] = user
            return self._json(200, {"token": token, "username": user})

        if path == "/api/login":
            user = data.get("username", "").strip()
            pwd = data.get("password", "")
            if user not in USERS:
                return self._json(401, {"error": "用户不存在"})
            if USERS[user] != hashlib.sha256(pwd.encode()).hexdigest():
                return self._json(401, {"error": "密码错误"})
            token = user + "_" + hashlib.md5(str(time.time()).encode()).hexdigest()
            TOKENS[token] = user
            return self._json(200, {"token": token, "username": user})

        self.send_error(404)


PUBLIC_MODE = True


def main():
    global PUBLIC_MODE, MUSIC_DIR, PORT
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--port" and i + 1 < len(args):
            PORT = int(args[i + 1])
            i += 2
        elif args[i] == "--music" and i + 1 < len(args):
            MUSIC_DIR = args[i + 1]
            i += 2
        elif args[i] == "--private":
            PUBLIC_MODE = False
            i += 1
        else:
            i += 1

    load_users()
    os.makedirs(MUSIC_DIR, exist_ok=True)
    refresh_library()
    print(f"MAKASOUNDER 社区服务器")
    print(f"  音乐目录: {os.path.abspath(MUSIC_DIR)}")
    print(f"  端口: {PORT}")
    print(f"  公开模式: {'是（无需登录）' if PUBLIC_MODE else '否（需要登录）'}")
    print(f"  访问: http://127.0.0.1:{PORT}/api/ping")
    print()

    server = HTTPServer(("0.0.0.0", PORT), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务器已停止")


if __name__ == "__main__":
    main()
