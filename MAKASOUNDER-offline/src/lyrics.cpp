#include "lyrics.h"

#include "common.h"
#include "fileio.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

namespace mk {

namespace {

bool LooksLikeTimeTag(const std::wstring& inside) {
    if (inside.size() < 4) return false;
    size_t colon = inside.find(L':');
    if (colon == std::wstring::npos || colon == 0) return false;
    for (size_t i = 0; i < colon; ++i) {
        if (!std::iswdigit(inside[i])) return false;
    }
    if (colon + 1 >= inside.size() || !std::iswdigit(inside[colon + 1])) return false;
    return true;
}

bool ParseTimestamp(const std::wstring& inside, int& timeMs) {
    size_t colon = inside.find(L':');
    if (colon == std::wstring::npos) return false;
    int minutes = 0;
    for (size_t i = 0; i < colon; ++i) {
        if (!std::iswdigit(inside[i])) return false;
        minutes = minutes * 10 + (inside[i] - L'0');
    }
    std::wstring rest = inside.substr(colon + 1);
    if (rest.empty()) return false;
    size_t dot = rest.find_first_of(L".:");
    int seconds = 0;
    int fraction = 0;
    int fractionDigits = 0;
    if (dot == std::wstring::npos) {
        for (wchar_t c : rest) {
            if (!std::iswdigit(c)) return false;
            seconds = seconds * 10 + (c - L'0');
        }
    } else {
        for (size_t i = 0; i < dot; ++i) {
            if (!std::iswdigit(rest[i])) return false;
            seconds = seconds * 10 + (rest[i] - L'0');
        }
        for (size_t i = dot + 1; i < rest.size(); ++i) {
            if (!std::iswdigit(rest[i])) return false;
            if (fractionDigits < 3) {
                fraction = fraction * 10 + (rest[i] - L'0');
                ++fractionDigits;
            }
        }
        if (fractionDigits == 1) fraction *= 100;
        else if (fractionDigits == 2) fraction *= 10;
    }
    timeMs = (minutes * 60 + seconds) * 1000 + fraction;
    return true;
}

}  // namespace

LrcParseResult LrcParser::Parse(const std::wstring& text) {
    LrcParseResult result;
    int offsetMs = 0;
    std::vector<std::pair<int, std::wstring>> entries;
    std::vector<std::wstring> plainLines;

    for (std::wstring line : SplitLines(text)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = Trim(line);
        if (line.empty()) continue;

        // Metadata tags such as [ti:...], [ar:...], [offset:...].
        if (line.front() == L'[') {
            size_t close = line.find(L']');
            if (close != std::wstring::npos) {
                std::wstring inside = line.substr(1, close - 1);
                size_t colon = inside.find(L':');
                if (colon != std::wstring::npos && !LooksLikeTimeTag(inside)) {
                    std::wstring key = ToLower(Trim(inside.substr(0, colon)));
                    std::wstring value = Trim(inside.substr(colon + 1));
                    if (key == L"offset") {
                        offsetMs = static_cast<int>(std::wcstol(value.c_str(), nullptr, 10));
                    }
                }
            }
        }

        std::vector<int> stamps;
        size_t cursor = 0;
        while (cursor < line.size() && line[cursor] == L'[') {
            size_t close = line.find(L']', cursor + 1);
            if (close == std::wstring::npos) break;
            std::wstring inside = line.substr(cursor + 1, close - cursor - 1);
            int stamp = 0;
            if (ParseTimestamp(inside, stamp)) {
                stamps.push_back(stamp);
                cursor = close + 1;
                while (cursor < line.size() && line[cursor] == L' ') ++cursor;
            } else {
                break;
            }
        }
        if (!stamps.empty()) {
            std::wstring lyricText = Trim(line.substr(cursor));
            for (int stamp : stamps) entries.emplace_back(stamp, lyricText);
        } else {
            if (!line.empty() && line.front() != L'[') plainLines.push_back(line);
        }
    }

    if (!entries.empty()) {
        std::stable_sort(entries.begin(), entries.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        for (auto& entry : entries) {
            LyricLine lyric;
            lyric.timeMs = std::max(0, entry.first + offsetMs);
            lyric.text = std::move(entry.second);
            result.lines.push_back(std::move(lyric));
        }
        result.timed = true;
    } else if (!plainLines.empty()) {
        for (auto& line : plainLines) {
            LyricLine lyric;
            lyric.timeMs = -1;
            lyric.text = std::move(line);
            result.lines.push_back(std::move(lyric));
        }
    }
    result.offsetMs = offsetMs;
    return result;
}

std::wstring LrcParser::FormatTimestamp(int timeMs) {
    if (timeMs < 0) return L"--:--.--";
    int total = timeMs;
    int minutes = total / 60000;
    int seconds = (total / 1000) % 60;
    int hundredths = (total % 1000) / 10;
    wchar_t buffer[32];
    _snwprintf(buffer, 32, L"[%02d:%02d.%02d]", minutes, seconds, hundredths);
    buffer[31] = L'\0';
    return buffer;
}

std::vector<std::wstring> SplitLyricPlainLines(const std::wstring& text) {
    std::vector<std::wstring> lines = SplitLines(text);
    std::vector<std::wstring> result;
    for (auto& line : lines) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = Trim(line);
        if (!line.empty()) result.push_back(line);
    }
    return result;
}

void LyricService::Clear() {
    lines_.clear();
    rawText_.clear();
    source_.clear();
    timed_ = false;
}

std::wstring LyricService::FindSidecar(const std::wstring& audioPath,
                                       const std::wstring& fileName) const {
    std::wstring dir = DirNameOf(audioPath);
    std::wstring direct = JoinPath(dir, fileName + L".lrc");
    if (FileExists(direct)) return direct;

    std::wstring pattern = JoinPath(dir, L"*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return {};
    std::wstring wanted = ToLower(fileName + L".lrc");
    std::wstring found;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (ToLower(data.cFileName) == wanted) {
            found = JoinPath(dir, data.cFileName);
            break;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return found;
}

bool LyricService::LoadText(std::wstring text, std::wstring source, bool forceTimed) {
    LrcParseResult parsed = LrcParser::Parse(text);
    if (parsed.lines.empty()) return false;
    rawText_ = std::move(text);
    source_ = std::move(source);
    timed_ = parsed.timed || forceTimed;
    if (!parsed.timed) {
        for (auto& line : parsed.lines) line.timeMs = -1;
    }
    lines_ = std::move(parsed.lines);
    return true;
}

bool LyricService::LoadForTrack(const std::wstring& audioPath, const std::wstring& titleTag,
                                const std::wstring& embeddedText) {
    Clear();
    std::wstring unsyncedFallback;
    if (!embeddedText.empty()) {
        LrcParseResult parsed = LrcParser::Parse(embeddedText);
        if (parsed.timed) {
            if (LoadText(embeddedText, L"内嵌同步歌词", true)) return true;
        } else if (!parsed.lines.empty()) {
            unsyncedFallback = embeddedText;
        }
    }

    std::wstring sidecar = FindSidecar(audioPath, StemOf(audioPath));
    if (!sidecar.empty()) {
        std::vector<uint8_t> bytes;
        if (ReadWholeFile(sidecar, bytes, 4u * 1024u * 1024u)) {
            std::wstring text = DecodeTextFileBytes(bytes);
            if (LoadText(text, L"同名 LRC", false)) return true;
        }
    }

    if (!titleTag.empty()) {
        std::wstring titleLrc = FindSidecar(audioPath, titleTag);
        if (!titleLrc.empty() && !IEquals(titleLrc, sidecar)) {
            std::vector<uint8_t> bytes;
            if (ReadWholeFile(titleLrc, bytes, 4u * 1024u * 1024u)) {
                std::wstring text = DecodeTextFileBytes(bytes);
                if (LoadText(text, L"标题 LRC", false)) return true;
            }
        }
    }

    if (!unsyncedFallback.empty()) {
        return LoadText(unsyncedFallback, L"内嵌歌词 · 未打轴", false);
    }
    return false;
}

int LyricService::IndexAtMs(int64_t positionMs) const {
    if (lines_.empty() || !timed_) return -1;
    int low = 0;
    int high = static_cast<int>(lines_.size()) - 1;
    int answer = -1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (lines_[mid].timeMs >= 0 && lines_[mid].timeMs <= positionMs) {
            answer = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return answer;
}

int LyricService::IndexAt(double positionSeconds) const {
    return IndexAtMs(static_cast<int64_t>(positionSeconds * 1000.0));
}

const LyricLine* LyricService::LineAt(int index) const {
    if (index < 0 || static_cast<size_t>(index) >= lines_.size()) return nullptr;
    return &lines_[index];
}

const LyricLine* LyricService::CurrentLine(double positionSeconds) const {
    return LineAt(IndexAt(positionSeconds));
}

}  // namespace mk

