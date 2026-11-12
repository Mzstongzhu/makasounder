#include "common.h"

#include <mmsystem.h>
#include <shlobj.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <cwctype>

namespace mk {

namespace {

bool IsValidUtf8(const std::string& text) {
    if (text.empty()) return true;
    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                       static_cast<int>(text.size()), nullptr, 0);
    return required > 0;
}

std::wstring MultiByteToWide(std::string_view text, UINT codePage, DWORD flags = 0) {
    if (text.empty()) return {};
    int required = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()),
                                       nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), result.data(),
                        required);
    return result;
}

}  // namespace

std::wstring Utf8ToWide(std::string_view text) {
    return MultiByteToWide(text, CP_UTF8, 0);
}

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    int required = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                       nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        required, nullptr, nullptr);
    return result;
}

std::wstring AnsiToWide(std::string_view text, UINT codePage) {
    return MultiByteToWide(text, codePage, 0);
}

std::wstring ToLower(std::wstring text) {
    for (wchar_t& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

bool IEquals(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    }
    return true;
}

std::wstring Trim(std::wstring text) {
    size_t begin = 0;
    while (begin < text.size() && std::iswspace(text[begin])) ++begin;
    size_t end = text.size();
    while (end > begin && std::iswspace(text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

std::vector<std::wstring> SplitLines(std::wstring_view text) {
    std::vector<std::wstring> result;
    std::wstring current;
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'\r' || c == L'\n') {
            result.push_back(current);
            current.clear();
            if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        } else {
            current.push_back(c);
        }
    }
    result.push_back(current);
    return result;
}

bool StartsWith(std::wstring_view text, std::wstring_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::wstring_view text, std::wstring_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::wstring ReplaceAll(std::wstring text, std::wstring_view from, std::wstring_view to) {
    if (from.empty()) return text;
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::wstring::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::wstring GetExePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring GetExeDir() { return DirNameOf(GetExePath()); }

std::wstring GetAppDataDir() {
    std::wstring base(MAX_PATH, L'\0');
    DWORD length = GetEnvironmentVariableW(L"APPDATA", base.data(), static_cast<DWORD>(base.size()));
    if (length == 0 || length >= base.size()) {
        base = GetExeDir();
    } else {
        base.resize(length);
        base = JoinPath(base, L"MAKASOUNDER");
        CreateDirectoryW(base.c_str(), nullptr);
    }
    return base;
}

std::wstring JoinPath(std::wstring_view dir, std::wstring_view name) {
    if (dir.empty()) return std::wstring(name);
    std::wstring result(dir);
    if (result.back() != L'\\' && result.back() != L'/') result.push_back(L'\\');
    result.append(name);
    return result;
}

std::wstring FileNameOf(std::wstring_view path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring_view::npos) return std::wstring(path);
    return std::wstring(path.substr(pos + 1));
}

std::wstring DirNameOf(std::wstring_view path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring_view::npos) return L".";
    if (pos == 0) return std::wstring(path.substr(0, 1));
    return std::wstring(path.substr(0, pos));
}

std::wstring StemOf(std::wstring_view path) {
    std::wstring name = FileNameOf(path);
    size_t pos = name.find_last_of(L'.');
    if (pos == std::wstring::npos || pos == 0) return name;
    return name.substr(0, pos);
}

std::wstring ExtensionOf(std::wstring_view path) {
    std::wstring name = FileNameOf(path);
    size_t pos = name.find_last_of(L'.');
    if (pos == std::wstring::npos) return {};
    return ToLower(name.substr(pos));
}

bool FileExists(std::wstring_view path) {
    DWORD attrs = GetFileAttributesW(std::wstring(path).c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirectoryExists(std::wstring_view path) {
    DWORD attrs = GetFileAttributesW(std::wstring(path).c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsSupportedAudio(std::wstring_view path) {
    static const wchar_t* kExtensions[] = {L".mp3",  L".flac", L".wav", L".m4a", L".aac",
                                           L".ogg",  L".opus", L".wma", L".ape", L".aiff",
                                           L".aif",  L".m4b",  L".wv",  L".mp4", L".oga"};
    std::wstring ext = ExtensionOf(path);
    for (const wchar_t* candidate : kExtensions) {
        if (ext == candidate) return true;
    }
    return false;
}

std::wstring FormatTime(double seconds, bool withHours) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    int64_t total = static_cast<int64_t>(seconds + 0.5);
    int64_t hours = total / 3600;
    int64_t minutes = (total / 60) % 60;
    int64_t secs = total % 60;
    wchar_t buffer[64];
    if (withHours || hours > 0) {
        _snwprintf(buffer, 64, L"%02lld:%02lld:%02lld", static_cast<long long>(hours),
                 static_cast<long long>(minutes), static_cast<long long>(secs));
    } else {
        _snwprintf(buffer, 64, L"%02lld:%02lld", static_cast<long long>(minutes),
                 static_cast<long long>(secs));
    }
    buffer[63] = L'\0';
    return buffer;
}

std::wstring FormatTimeMs(int64_t milliseconds) {
    if (milliseconds < 0) return L"--:--.--";
    return FormatTime(static_cast<double>(milliseconds) / 1000.0);
}

std::wstring ErrorMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0,
                                  nullptr);
    std::wstring result;
    if (length && buffer) {
        result.assign(buffer, length);
        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' ||
                                   result.back() == L' '))
            result.pop_back();
    }
    if (buffer) LocalFree(buffer);
    if (result.empty()) {
        wchar_t fallback[32];
        _snwprintf(fallback, 32, L"错误 %lu", static_cast<unsigned long>(code));
        fallback[31] = L'\0';
        result = fallback;
    }
    return result;
}

std::wstring MciErrorMessage(DWORD code) {
    wchar_t buffer[512] = {};
    if (mciGetErrorStringW(code, buffer, 512)) return buffer;
    return ErrorMessage(code);
}

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes) {
    out.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<uint64_t>(size.QuadPart) > maxBytes) {
        CloseHandle(file);
        return false;
    }
    out.resize(static_cast<size_t>(size.QuadPart));
    size_t offset = 0;
    while (offset < out.size()) {
        DWORD read = 0;
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(out.size() - offset, 1u << 20));
        if (!ReadFile(file, out.data() + offset, chunk, &read, nullptr) || read == 0) {
            CloseHandle(file);
            out.clear();
            return false;
        }
        offset += read;
    }
    CloseHandle(file);
    return true;
}

bool WriteUtf8File(const std::wstring& path, std::wstring_view text, bool bom) {
    std::string utf8;
    if (bom) utf8.append("\xEF\xBB\xBF");
    utf8 += WideToUtf8(text);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    size_t offset = 0;
    bool ok = true;
    while (offset < utf8.size()) {
        DWORD written = 0;
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(utf8.size() - offset, 1u << 20));
        if (!WriteFile(file, utf8.data() + offset, chunk, &written, nullptr) || written == 0) {
            ok = false;
            break;
        }
        offset += written;
    }
    CloseHandle(file);
    return ok;
}

std::wstring DecodeTextFileBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return {};
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return Utf8ToWide(std::string_view(reinterpret_cast<const char*>(bytes.data() + 3),
                                           bytes.size() - 3));
    }
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        size_t count = (bytes.size() - 2) / 2;
        std::wstring result(count, L'\0');
        if (count) {
            memcpy(result.data(), bytes.data() + 2, count * 2);
        }
        return result;
    }
    if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        size_t count = (bytes.size() - 2) / 2;
        std::wstring result(count, L'\0');
        for (size_t i = 0; i < count; ++i) {
            uint16_t v = static_cast<uint16_t>(bytes[2 + i * 2]) << 8 |
                         static_cast<uint16_t>(bytes[3 + i * 2]);
            result[i] = static_cast<wchar_t>(v);
        }
        return result;
    }
    std::string raw(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (IsValidUtf8(raw)) return Utf8ToWide(raw);
    UINT codePage = 936;  // A common legacy encoding for Chinese LRC files.
    wchar_t lang[16] = {};
    if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, lang, 16) > 0) {
        int cp = static_cast<int>(std::wcstol(lang, nullptr, 10));
        if (cp > 0) codePage = static_cast<UINT>(cp);
    }
    return AnsiToWide(raw, codePage);
}

double NowSeconds() {
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}

std::wstring NumberText(int value) {
    wchar_t buffer[32];
    _snwprintf(buffer, 32, L"%d", value);
    buffer[31] = 0;
    return buffer;
}

}  // namespace mk

