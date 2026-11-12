#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mk {

constexpr wchar_t kAppName[] = L"MAKASOUNDER";
constexpr wchar_t kAppVersion[] = L"1.0.0";

std::wstring Utf8ToWide(std::string_view text);
std::string WideToUtf8(std::wstring_view text);
std::wstring AnsiToWide(std::string_view text, UINT codePage = CP_ACP);
std::wstring ToLower(std::wstring text);
bool IEquals(std::wstring_view a, std::wstring_view b);
std::wstring Trim(std::wstring text);
std::vector<std::wstring> SplitLines(std::wstring_view text);
bool StartsWith(std::wstring_view text, std::wstring_view prefix);
bool EndsWith(std::wstring_view text, std::wstring_view suffix);
std::wstring ReplaceAll(std::wstring text, std::wstring_view from, std::wstring_view to);

std::wstring GetExePath();
std::wstring GetExeDir();
std::wstring GetAppDataDir();
std::wstring JoinPath(std::wstring_view dir, std::wstring_view name);
std::wstring FileNameOf(std::wstring_view path);
std::wstring DirNameOf(std::wstring_view path);
std::wstring StemOf(std::wstring_view path);
std::wstring ExtensionOf(std::wstring_view path);
bool FileExists(std::wstring_view path);
bool DirectoryExists(std::wstring_view path);
bool IsSupportedAudio(std::wstring_view path);

std::wstring FormatTime(double seconds, bool withHours = false);
std::wstring FormatTimeMs(int64_t milliseconds);
std::wstring ErrorMessage(DWORD code);
std::wstring MciErrorMessage(DWORD code);

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes = 8u * 1024u * 1024u);
bool WriteUtf8File(const std::wstring& path, std::wstring_view text, bool bom = true);
std::wstring DecodeTextFileBytes(const std::vector<uint8_t>& bytes);

double NowSeconds();

template <typename T>
T ClampValue(T value, T low, T high) {
    return value < low ? low : (value > high ? high : value);
}

}  // namespace mk

