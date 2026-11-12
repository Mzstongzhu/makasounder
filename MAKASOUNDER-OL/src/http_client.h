#pragma once

#include <string>
#include <vector>

namespace mk {

struct HttpResponse {
    int status = 0;
    std::string body;          // raw bytes for binary downloads
    std::wstring text;         // decoded body for JSON/text
    std::string contentType;
    std::wstring error;
    bool ok = false;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    void SetServer(const std::wstring& baseUrl);
    void SetToken(const std::wstring& token);
    void SetTimeoutMs(int ms);

    HttpResponse Get(const std::wstring& path);
    HttpResponse Download(const std::wstring& path, const std::wstring& savePath);
    HttpResponse Post(const std::wstring& path, const std::string& body,
                      const std::string& contentType = "application/json");

    bool IsConnected() const { return connected_; }

private:
    HttpResponse DoRequest(const std::wstring& method, const std::wstring& path,
                           const std::string& body, const std::wstring& savePath);

    std::wstring server_;
    std::wstring host_;
    int port_ = 80;
    bool useHttps_ = false;
    std::wstring token_;
    int timeoutMs_ = 15000;
    bool connected_ = false;
};

std::wstring UrlEncode(const std::wstring& text);

}  // namespace mk
