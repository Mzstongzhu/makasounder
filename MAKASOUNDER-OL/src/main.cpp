#include "app.h"

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool startFloat = false;
    std::vector<std::wstring> files;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--float" || arg == L"/float" || arg == L"-float") {
                startFloat = true;
            } else if (!arg.empty() && arg[0] == L'-') {
                continue;
            } else {
                files.push_back(arg);
            }
        }
        LocalFree(argv);
    }

    mk::App app;
    return app.Run(instance, startFloat, files);
}
