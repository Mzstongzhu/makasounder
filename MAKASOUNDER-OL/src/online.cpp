#include "online.h"

#include "common.h"
#include "json.h"

#include <sstream>

namespace mk {

OnlineService::OnlineService() {
    std::wstring cache = JoinPath(GetAppDataDir(), L"cache");
    CreateDirectoryW(cache.c_str(), nullptr);
}

void OnlineService::SetServer(const std::wstring& baseUrl) {
    server_ = baseUrl;
    token_.clear();
    username_.clear();
    http_.SetServer(baseUrl);
}

void OnlineService::SetToken(const std::wstring& token) {
    token_ = token;
    http_.SetToken(token);
}

void OnlineService::ClearToken() {
    token_.clear();
    username_.clear();
    http_.SetToken(L"");
}

std::wstring OnlineService::CacheDir() const {
    return JoinPath(GetAppDataDir(), L"cache");
}

bool OnlineService::TestConnection(std::wstring& error) {
    HttpResponse response = http_.Get(L"/api/ping");
    if (!response.ok) {
        error = response.error.empty() ? L"无法连接到服务器" : response.error;
        return false;
    }
    return true;
}

bool OnlineService::Login(const std::wstring& username, const std::wstring& password,
                          std::wstring& error) {
    std::wstringstream body;
    body << L"{\"username\":\"" << username << L"\",\"password\":\"" << password << L"\"}";
    HttpResponse response = http_.Post(L"/api/login", WideToUtf8(body.str()));
    if (!response.ok) {
        error = response.error.empty() ? L"登录失败" : response.error;
        if (!response.text.empty()) {
            JsonValue json = JsonParser::Parse(WideToUtf8(response.text));
            std::wstring msg = json.String("error");
            if (!msg.empty()) error = msg;
        }
        return false;
    }
    JsonValue json = JsonParser::Parse(WideToUtf8(response.text));
    token_ = json.String("token");
    username_ = json.String("username");
    if (token_.empty()) {
        error = L"服务器未返回 token";
        return false;
    }
    http_.SetToken(token_);
    return true;
}

bool OnlineService::Register(const std::wstring& username, const std::wstring& password,
                             std::wstring& error) {
    std::wstringstream body;
    body << L"{\"username\":\"" << username << L"\",\"password\":\"" << password << L"\"}";
    HttpResponse response = http_.Post(L"/api/register", WideToUtf8(body.str()));
    if (!response.ok) {
        error = response.error.empty() ? L"注册失败" : response.error;
        if (!response.text.empty()) {
            JsonValue json = JsonParser::Parse(WideToUtf8(response.text));
            std::wstring msg = json.String("error");
            if (!msg.empty()) error = msg;
        }
        return false;
    }
    return Login(username, password, error);
}

std::vector<OnlineTrack> OnlineService::Search(const std::wstring& query,
                                               std::wstring& error) {
    std::vector<OnlineTrack> results;
    if (query.empty()) return results;
    std::wstring path = L"/api/search?q=" + UrlEncode(query);
    HttpResponse response = http_.Get(path);
    if (!response.ok) {
        error = response.error.empty() ? L"搜索失败" : response.error;
        return results;
    }
    JsonValue json = JsonParser::Parse(WideToUtf8(response.text));
    if (!json.IsArray()) return results;
    for (const auto& item : json.arrayValue) {
        OnlineTrack track;
        track.id = item.String("id");
        track.title = item.String("title");
        track.artist = item.String("artist");
        track.album = item.String("album");
        track.duration = item.String("duration");
        track.downloadUrl = item.String("download");
        track.streamUrl = item.String("stream");
        track.ext = item.String("ext");
        track.hasLyrics = item.Number("lyrics", 0) != 0;
        track.hasArtwork = item.Number("artwork", 0) != 0;
        if (track.id.empty()) continue;
        results.push_back(std::move(track));
    }
    return results;
}

bool OnlineService::DownloadTrack(const OnlineTrack& track, std::wstring& savePath,
                                  std::wstring& error) {
    if (track.downloadUrl.empty()) {
        error = L"该曲目没有可下载的地址";
        return false;
    }
    std::wstring filename = track.title.empty() ? track.id : track.title;
    for (wchar_t& c : filename) {
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' || c == L'"' ||
            c == L'<' || c == L'>' || c == L'|')
            c = L'_';
    }
    if (filename.size() > 80) filename = filename.substr(0, 80);
    std::wstring ext = track.ext.empty() ? L".mp3" : ToLower(track.ext);
    if (ext.empty() || ext[0] != L'.') ext = L"." + ext;
    savePath = JoinPath(CacheDir(), filename + L"_" + track.id + ext);
    if (FileExists(savePath)) return true;
    HttpResponse response = http_.Download(track.downloadUrl, savePath);
    if (!response.ok) {
        error = response.error.empty() ? L"下载失败" : response.error;
        DeleteFileW(savePath.c_str());
        savePath.clear();
        return false;
    }
    return true;
}

std::wstring OnlineService::DownloadLyrics(const OnlineTrack& track, std::wstring& error) {
    if (!track.hasLyrics) return {};
    std::wstring path = L"/api/lyrics/" + track.id;
    HttpResponse response = http_.Get(path);
    if (!response.ok) {
        error = response.error;
        return {};
    }
    return response.text;
}

std::wstring OnlineService::DownloadArtwork(const OnlineTrack& track, std::wstring& error) {
    if (!track.hasArtwork) return {};
    std::wstring path = L"/api/artwork/" + track.id;
    std::wstring filename = track.id + L".jpg";
    std::wstring savePath = JoinPath(CacheDir(), filename);
    if (FileExists(savePath)) return savePath;
    HttpResponse response = http_.Download(path, savePath);
    if (!response.ok) {
        error = response.error;
        DeleteFileW(savePath.c_str());
        return {};
    }
    return savePath;
}

}  // namespace mk
