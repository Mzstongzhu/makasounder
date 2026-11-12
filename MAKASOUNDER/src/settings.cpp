#include "settings.h"

#include "common.h"

#include <cstdlib>
#include <map>
#include <sstream>

namespace mk {

namespace {

int ToInt(const std::wstring& value, int fallback) {
    if (value.empty()) return fallback;
    wchar_t* end = nullptr;
    long parsed = std::wcstol(value.c_str(), &end, 10);
    if (end == value.c_str()) return fallback;
    return static_cast<int>(parsed);
}

bool ToBool(const std::wstring& value, bool fallback) {
    if (value == L"1" || value == L"true" || value == L"yes") return true;
    if (value == L"0" || value == L"false" || value == L"no") return false;
    return fallback;
}

std::wstring FromBool(bool value) { return value ? L"1" : L"0"; }

}  // namespace

bool SettingsStore::Load() {
    settings_ = Settings{};
    path_ = JoinPath(GetAppDataDir(), L"settings.ini");
    std::vector<uint8_t> bytes;
    if (!ReadWholeFile(path_, bytes, 1024u * 1024u)) return false;
    std::wstring text = DecodeTextFileBytes(bytes);
    for (std::wstring line : SplitLines(text)) {
        line = Trim(line);
        if (line.empty() || line[0] == L'#') continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = Trim(line.substr(0, eq));
        std::wstring value = Trim(line.substr(eq + 1));
        if (key == L"volume") settings_.volume = ClampValue(ToInt(value, 70), 0, 100);
        else if (key == L"snap") settings_.snapEnabled = ToBool(value, true);
        else if (key == L"peek") settings_.peekEnabled = ToBool(value, true);
        else if (key == L"double_click_float") settings_.doubleClickFloat = ToBool(value, false);
        else if (key == L"has_float_pos") settings_.hasFloatPos = ToBool(value, false);
        else if (key == L"float_x") settings_.floatX = ToInt(value, 0);
        else if (key == L"float_y") settings_.floatY = ToInt(value, 0);
        else if (key == L"float_compact") settings_.floatCompact = ToBool(value, false);
        else if (key == L"has_main_rect") settings_.hasMainRect = ToBool(value, false);
        else if (key == L"main_x") settings_.mainRect.left = ToInt(value, 100);
        else if (key == L"main_y") settings_.mainRect.top = ToInt(value, 80);
        else if (key == L"main_w") settings_.mainRect.right = ToInt(value, 1240);
        else if (key == L"main_h") settings_.mainRect.bottom = ToInt(value, 820);
        else if (key == L"last_dir") settings_.lastDir = value;
    }
    if (settings_.mainRect.right < 600) settings_.mainRect.right = 1240;
    if (settings_.mainRect.bottom < 500) settings_.mainRect.bottom = 820;
    return true;
}

bool SettingsStore::Save() const {
    std::wstringstream output;
    output << L"# MAKASOUNDER settings\n";
    output << L"volume=" << settings_.volume << L"\n";
    output << L"snap=" << FromBool(settings_.snapEnabled) << L"\n";
    output << L"peek=" << FromBool(settings_.peekEnabled) << L"\n";
    output << L"double_click_float=" << FromBool(settings_.doubleClickFloat) << L"\n";
    output << L"has_float_pos=" << FromBool(settings_.hasFloatPos) << L"\n";
    output << L"float_x=" << settings_.floatX << L"\n";
    output << L"float_y=" << settings_.floatY << L"\n";
    output << L"float_compact=" << FromBool(settings_.floatCompact) << L"\n";
    output << L"has_main_rect=" << FromBool(settings_.hasMainRect) << L"\n";
    output << L"main_x=" << settings_.mainRect.left << L"\n";
    output << L"main_y=" << settings_.mainRect.top << L"\n";
    output << L"main_w=" << (settings_.mainRect.right - settings_.mainRect.left) << L"\n";
    output << L"main_h=" << (settings_.mainRect.bottom - settings_.mainRect.top) << L"\n";
    output << L"last_dir=" << settings_.lastDir << L"\n";
    return WriteUtf8File(path_, output.str(), false);
}

}  // namespace mk
