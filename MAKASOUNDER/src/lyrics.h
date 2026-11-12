#pragma once

#include <string>
#include <vector>

namespace mk {

struct LyricLine {
    int timeMs = -1;
    std::wstring text;
};

struct LrcParseResult {
    std::vector<LyricLine> lines;
    bool timed = false;
    int offsetMs = 0;
};

class LrcParser {
public:
    static LrcParseResult Parse(const std::wstring& text);
    static std::wstring FormatTimestamp(int timeMs);
};

class LyricService {
public:
    void Clear();
    bool LoadForTrack(const std::wstring& audioPath, const std::wstring& titleTag,
                      const std::wstring& embeddedText);

    int IndexAt(double positionSeconds) const;
    int IndexAtMs(int64_t positionMs) const;
    const LyricLine* LineAt(int index) const;
    const LyricLine* CurrentLine(double positionSeconds) const;

    bool HasAny() const { return !lines_.empty(); }
    bool IsTimed() const { return timed_; }
    size_t LineCount() const { return lines_.size(); }
    const std::vector<LyricLine>& Lines() const { return lines_; }
    std::vector<LyricLine>& MutableLines() { return lines_; }
    const std::wstring& RawText() const { return rawText_; }
    const std::wstring& Source() const { return source_; }
    void SetSource(const std::wstring& source) { source_ = source; }
    void SetTimed(bool timed) { timed_ = timed; }
    void SetRawText(std::wstring text) { rawText_ = std::move(text); }

private:
    bool LoadText(std::wstring text, std::wstring source, bool forceTimed);
    std::wstring FindSidecar(const std::wstring& audioPath, const std::wstring& fileName) const;

    std::vector<LyricLine> lines_;
    std::wstring rawText_;
    std::wstring source_;
    bool timed_ = false;
};

std::vector<std::wstring> SplitLyricPlainLines(const std::wstring& text);

}  // namespace mk
