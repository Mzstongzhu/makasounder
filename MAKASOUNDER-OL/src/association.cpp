#include "association.h"

#include "common.h"

#include <shlobj.h>
#include <shlwapi.h>

namespace mk {

namespace {

constexpr wchar_t kProgId[] = L"MAKASOUNDER.mp3";
constexpr wchar_t kProgIdPath[] = L"Software\\Classes\\MAKASOUNDER.mp3";
constexpr wchar_t kMp3Key[] = L"Software\\Classes\\.mp3";
constexpr wchar_t kApplicationKey[] = L"Software\\Classes\\Applications\\MAKASOUNDER.exe";

bool SetValue(HKEY root, const std::wstring& subKey, const wchar_t* valueName,
              const std::wstring& value, DWORD type, std::wstring& error) {
    HKEY key = nullptr;
    LONG status = RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                                     nullptr);
    if (status != ERROR_SUCCESS) {
        error = ErrorMessage(static_cast<DWORD>(status));
        return false;
    }
    const BYTE* data = reinterpret_cast<const BYTE*>(value.c_str());
    DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    if (type == REG_NONE || type == REG_SZ) {
        status = RegSetValueExW(key, valueName, 0, type, data, bytes);
    } else {
        status = RegSetValueExW(key, valueName, 0, type, data, bytes);
    }
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        error = ErrorMessage(static_cast<DWORD>(status));
        return false;
    }
    return true;
}

bool SetDefault(HKEY root, const std::wstring& subKey, const std::wstring& value,
                std::wstring& error) {
    return SetValue(root, subKey, nullptr, value, REG_SZ, error);
}

bool SetNamed(HKEY root, const std::wstring& subKey, const wchar_t* name,
              const std::wstring& value, std::wstring& error) {
    return SetValue(root, subKey, name, value, REG_SZ, error);
}

std::wstring ReadString(HKEY root, const std::wstring& subKey, const wchar_t* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return {};
    wchar_t buffer[4096] = {};
    DWORD bytes = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    LONG status = RegQueryValueExW(key, valueName, nullptr, &type,
                                      reinterpret_cast<BYTE*>(buffer), &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    buffer[bytes / sizeof(wchar_t)] = L'\0';
    return buffer;
}

}  // namespace

AssociationStatus QueryAssociation() {
    AssociationStatus status;
    std::wstring defaultValue = ReadString(HKEY_CURRENT_USER, kMp3Key, nullptr);
    std::wstring command = ReadString(HKEY_CURRENT_USER, std::wstring(kProgIdPath) +
                                                             L"\\shell\\open\\command",
                                      nullptr);
    status.command = command;
    status.registered = IEquals(defaultValue, kProgId);
    status.floatMode = command.find(L"--float") != std::wstring::npos;
    return status;
}

bool RegisterAssociation(bool doubleClickFloat, std::wstring& error) {
    error.clear();
    std::wstring exe = GetExePath();
    if (exe.empty()) {
        error = L"无法获取程序路径";
        return false;
    }
    std::wstring command = L"\"" + exe + L"\" ";
    if (doubleClickFloat) command += L"--float ";
    command += L"\"%1\"";
    std::wstring icon = L"\"" + exe + L"\",0";

    if (!SetDefault(HKEY_CURRENT_USER, kProgIdPath, L"MAKASOUNDER 音频文件", error)) return false;
    if (!SetDefault(HKEY_CURRENT_USER, std::wstring(kProgIdPath) + L"\\DefaultIcon", icon, error))
        return false;
    if (!SetDefault(HKEY_CURRENT_USER, std::wstring(kProgIdPath) + L"\\shell\\open\\command",
                    command, error))
        return false;
    if (!SetDefault(HKEY_CURRENT_USER, kMp3Key, kProgId, error)) return false;
    if (!SetNamed(HKEY_CURRENT_USER, std::wstring(kMp3Key) + L"\\OpenWithProgids", kProgId, L"",
                  error))
        return false;
    if (!SetDefault(HKEY_CURRENT_USER, kApplicationKey,
                    L"MAKASOUNDER 离线音乐播放器", error))
        return false;
    if (!SetDefault(HKEY_CURRENT_USER, std::wstring(kApplicationKey) + L"\\shell\\open\\command",
                    command, error))
        return false;
    if (!SetDefault(HKEY_CURRENT_USER,
                    std::wstring(kApplicationKey) + L"\\SupportedTypes\\.mp3", L"", error))
        return false;
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool RemoveAssociation(std::wstring& error) {
    error.clear();
    std::wstring current = ReadString(HKEY_CURRENT_USER, kMp3Key, nullptr);
    if (IEquals(current, kProgId)) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kMp3Key, 0, KEY_WRITE, &key) == ERROR_SUCCESS) {
            RegDeleteValueW(key, nullptr);
            RegCloseKey(key);
        }
    }
    HKEY openWith = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, (std::wstring(kMp3Key) + L"\\OpenWithProgids").c_str(), 0,
                      KEY_WRITE, &openWith) == ERROR_SUCCESS) {
        RegDeleteValueW(openWith, kProgId);
        RegCloseKey(openWith);
    }
    SHDeleteKeyW(HKEY_CURRENT_USER, kProgIdPath);
    SHDeleteKeyW(HKEY_CURRENT_USER, kApplicationKey);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

}  // namespace mk
