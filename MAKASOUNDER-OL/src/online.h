#pragma once

#include <memory>
#include <string>
#include <vector>

#include "http_client.h"

namespace mk {

struct OnlineTrack {
    std::wstring id;
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring duration;      // "mm:ss"
    std::wstring downloadUrl;   // path on server
    std::wstring streamUrl;     // path for direct streaming
    std::wstring ext;           // source extension, e.g. ".mp3"
    bool hasLyrics = false;
    bool hasArtwork = false;
    std::wstring cachedPath;    // local file after download
};

class OnlineService {
public:
    OnlineService();

    void SetServer(const std::wstring& baseUrl);
    void SetToken(const std::wstring& token);
    void ClearToken();

    bool TestConnection(std::wstring& error);
    bool Login(const std::wstring& username, const std::wstring& password,
               std::wstring& error);
    bool Register(const std::wstring& username, const std::wstring& password,
                  std::wstring& error);

    std::vector<OnlineTrack> Search(const std::wstring& query, std::wstring& error);
    bool DownloadTrack(const OnlineTrack& track, std::wstring& savePath,
                       std::wstring& error);
    std::wstring DownloadLyrics(const OnlineTrack& track, std::wstring& error);
    std::wstring DownloadArtwork(const OnlineTrack& track, std::wstring& error);

    const std::wstring& Server() const { return server_; }
    const std::wstring& Token() const { return token_; }
    const std::wstring& Username() const { return username_; }
    bool IsLoggedIn() const { return !token_.empty(); }
    bool IsConfigured() const { return !server_.empty(); }

    std::wstring CacheDir() const;

private:
    std::wstring server_;
    std::wstring token_;
    std::wstring username_;
    HttpClient http_;
};

}  // namespace mk
