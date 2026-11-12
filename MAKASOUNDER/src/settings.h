#pragma once

#include <windows.h>

#include <string>

namespace mk {

struct Settings {
    int volume = 70;
    bool snapEnabled = true;
    bool peekEnabled = true;
    bool doubleClickFloat = false;
    bool hasFloatPos = false;
    int floatX = 0;
    int floatY = 0;
    bool floatCompact = false;
    bool hasMainRect = false;
    RECT mainRect{100, 80, 1340, 900};
    std::wstring lastDir;
};

class SettingsStore {
public:
    bool Load();
    bool Save() const;

    Settings& Data() { return settings_; }
    const Settings& Data() const { return settings_; }
    const std::wstring& Path() const { return path_; }

private:
    Settings settings_;
    std::wstring path_;
};

}  // namespace mk
