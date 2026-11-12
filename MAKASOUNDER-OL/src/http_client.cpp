#include "http_client.h"

#include "common.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace mk {

namespace {

struct WinsockInit {
    WinsockInit() {
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WinsockInit() { WSACleanup(); }
};

WinsockInit& GlobalWinsock() {
    static WinsockInit init;
    return init;
}

void ParseUrl(const std::wstring& url, std::wstring& host, int& port, bool& https,
              std::wstring& path) {
    host.clear();
    path = L"/";
    port = 80;
    https = false;
    std::wstring rest = url;
    if (rest.size() >= 8 && ToLower(rest.substr(0, 8)) == L"https://") {
        https = true;
        port = 443;
        rest = rest.substr(8);
    } else if (rest.size() >= 7 && ToLower(rest.substr(0, 7)) == L"http://") {
        rest = rest.substr(7);
    }
    size_t slash = rest.find(L'/');
    std::wstring authority = slash == std::wstring::npos ? rest : rest.substr(0, slash);
    if (slash != std::wstring::npos) path = rest.substr(slash);
    size_t colon = authority.find(L':');
    if (colon != std::wstring::npos) {
        host = authority.substr(0, colon);
        port = static_cast<int>(std::wcstol(authority.substr(colon + 1).c_str(), nullptr, 10));
        if (port <= 0) port = https ? 443 : 80;
    } else {
        host = authority;
    }
    if (host.empty()) {
        host = L"localhost";
    }
}

bool RecvAll(SOCKET sock, std::vector<char>& buffer, int timeoutMs) {
    buffer.clear();
    char chunk[8192];
    for (;;) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        timeval timeout{};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
        if (selectResult <= 0) return !buffer.empty();
        int received = recv(sock, chunk, sizeof(chunk), 0);
        if (received <= 0) break;
        buffer.insert(buffer.end(), chunk, chunk + received);
        if (buffer.size() > 64u * 1024u * 1024u) break;
    }
    return !buffer.empty();
}

std::string ParseHeaders(const std::vector<char>& raw, size_t& headerEnd,
                         std::string& contentType, int& status) {
    headerEnd = 0;
    status = 0;
    contentType.clear();
    for (size_t i = 0; i + 3 < raw.size(); ++i) {
        if (raw[i] == '\r' && raw[i + 1] == '\n' && raw[i + 2] == '\r' &&
            raw[i + 3] == '\n') {
            headerEnd = i + 4;
            break;
        }
        if (raw[i] == '\n' && raw[i + 1] == '\n') {
            headerEnd = i + 2;
            break;
        }
    }
    if (headerEnd == 0) return {};
    std::string headers(raw.begin(), raw.begin() + headerEnd);
    if (headers.size() >= 12 && headers.substr(0, 5) == "HTTP/") {
        size_t space = headers.find(' ');
        if (space != std::string::npos) {
            status = atoi(headers.c_str() + space + 1);
        }
    }
    std::string lower = headers;
    for (char& c : lower)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    size_t ct = lower.find("content-type:");
    if (ct != std::string::npos) {
        size_t end = lower.find("\r\n", ct);
        if (end == std::string::npos) end = lower.find('\n', ct);
        contentType = headers.substr(ct + 13, end == std::string::npos ? std::string::npos : end - ct - 13);
        while (!contentType.empty() && (contentType.front() == ' ' || contentType.front() == '\t'))
            contentType.erase(0, 1);
        while (!contentType.empty() && (contentType.back() == '\r' || contentType.back() == '\n' ||
                                        contentType.back() == ' '))
            contentType.pop_back();
    }
    return headers;
}

}  // namespace

std::wstring UrlEncode(const std::wstring& text) {
    std::string utf8 = WideToUtf8(text);
    std::wstring result;
    wchar_t hex[8];
    for (unsigned char c : utf8) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            result.push_back(static_cast<wchar_t>(c));
        } else {
            _snwprintf(hex, 8, L"%%%02X", c);
            hex[7] = 0;
            result += hex;
        }
    }
    return result;
}

HttpClient::HttpClient() { GlobalWinsock(); }
HttpClient::~HttpClient() {}

void HttpClient::SetServer(const std::wstring& baseUrl) {
    server_ = baseUrl;
    std::wstring discard;
    ParseUrl(baseUrl, host_, port_, useHttps_, discard);
    connected_ = !host_.empty();
}

void HttpClient::SetToken(const std::wstring& token) { token_ = token; }
void HttpClient::SetTimeoutMs(int ms) { timeoutMs_ = ms < 1000 ? 1000 : ms; }

HttpResponse HttpClient::Get(const std::wstring& path) {
    return DoRequest(L"GET", path, {}, {});
}

HttpResponse HttpClient::Download(const std::wstring& path, const std::wstring& savePath) {
    return DoRequest(L"GET", path, {}, savePath);
}

HttpResponse HttpClient::Post(const std::wstring& path, const std::string& body,
                              const std::string& contentType) {
    HttpResponse response = DoRequest(L"POST", path, body, {});
    if (!contentType.empty()) {
        (void)contentType;
    }
    return response;
}

HttpResponse HttpClient::DoRequest(const std::wstring& method, const std::wstring& path,
                                   const std::string& body, const std::wstring& savePath) {
    HttpResponse response;
    if (host_.empty()) {
        response.error = L"未配置服务器";
        return response;
    }
    if (useHttps_) {
        response.error = L"在线版暂不支持 HTTPS，请使用 HTTP 服务器";
        response.status = 0;
        return response;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    std::string hostUtf8 = WideToUtf8(host_);
    std::string portStr = std::to_string(port_);
    addrinfo* results = nullptr;
    if (getaddrinfo(hostUtf8.c_str(), portStr.c_str(), &hints, &results) != 0 || !results) {
        response.error = L"无法解析服务器地址：" + host_;
        return response;
    }
    SOCKET sock = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(results);
        response.error = L"无法创建网络套接字";
        return response;
    }

    DWORD timeout = static_cast<DWORD>(timeoutMs_);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));

    if (connect(sock, results->ai_addr, static_cast<int>(results->ai_addrlen)) == SOCKET_ERROR) {
        freeaddrinfo(results);
        closesocket(sock);
        response.error = L"无法连接到服务器 " + host_ + L":" + NumberText(port_);
        return response;
    }
    freeaddrinfo(results);

    std::string request = WideToUtf8(method);
    request += " ";
    request += WideToUtf8(path);
    request += " HTTP/1.1\r\nHost: ";
    request += hostUtf8;
    request += "\r\nUser-Agent: MAKASOUNDER-OL/1.0\r\nConnection: close\r\n";
    if (!token_.empty()) {
        request += "Authorization: Bearer ";
        request += WideToUtf8(token_);
        request += "\r\n";
    }
    if (!body.empty()) {
        request += "Content-Type: application/json; charset=utf-8\r\nContent-Length: ";
        request += std::to_string(body.size());
        request += "\r\n\r\n";
        request += body;
    } else {
        request += "\r\n";
    }

    int sent = 0;
    int total = static_cast<int>(request.size());
    while (sent < total) {
        int chunk = send(sock, request.data() + sent, total - sent, 0);
        if (chunk <= 0) {
            closesocket(sock);
            response.error = L"发送请求失败";
            return response;
        }
        sent += chunk;
    }

    std::vector<char> raw;
    if (!RecvAll(sock, raw, timeoutMs_)) {
        closesocket(sock);
        response.error = L"接收数据超时";
        return response;
    }
    closesocket(sock);

    size_t headerEnd = 0;
    std::string headers = ParseHeaders(raw, headerEnd, response.contentType, response.status);
    (void)headers;
    if (headerEnd == 0) {
        response.error = L"服务器响应格式错误";
        return response;
    }
    response.body.assign(raw.begin() + headerEnd, raw.end());

    if (!savePath.empty()) {
        HANDLE file = CreateFileW(savePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            response.error = L"无法写入下载文件";
            return response;
        }
        size_t offset = 0;
        bool ok = true;
        while (offset < response.body.size()) {
            DWORD written = 0;
            DWORD chunk = static_cast<DWORD>(std::min<size_t>(response.body.size() - offset, 1u << 20));
            if (!WriteFile(file, response.body.data() + offset, chunk, &written, nullptr) ||
                written != chunk) {
                ok = false;
                break;
            }
            offset += written;
        }
        CloseHandle(file);
        if (!ok) {
            response.error = L"下载写入失败";
            return response;
        }
    }

    bool isText = response.contentType.find("json") != std::string::npos ||
                  response.contentType.find("text") != std::string::npos ||
                  response.contentType.empty();
    if (isText) response.text = Utf8ToWide(response.body);

    response.ok = response.status >= 200 && response.status < 300;
    if (!response.ok && response.error.empty()) {
        response.error = L"服务器返回状态 " + NumberText(response.status);
    }
    return response;
}

}  // namespace mk
