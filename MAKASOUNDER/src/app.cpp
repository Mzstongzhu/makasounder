#include "app.h"

#include "common.h"
#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <set>
#include <sstream>

namespace mk {

namespace {

constexpr UINT_PTR kTimerTick = 1;
constexpr size_t kAutoScanPromptLimit = 200;

std::wstring GetFullPath(const std::wstring& path) {
    wchar_t buffer[32768];
    DWORD length = GetFullPathNameW(path.c_str(), 32768, buffer, nullptr);
    if (length == 0 || length >= 32768) return path;
    return buffer;
}

std::wstring NumberText(int value) {
    wchar_t buffer[32];
    _snwprintf(buffer, 32, L"%d", value);
    buffer[31] = 0;
    return buffer;
}

void CollectAudioFiles(const std::wstring& directory, std::vector<std::wstring>& out,
                       int depth = 0) {
    if (depth > 12 || out.size() > 5000) return;
    std::wstring pattern = JoinPath(directory, L"*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> dirs;
    std::vector<std::wstring> files;
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dirs.push_back(data.cFileName);
        } else if (IsSupportedAudio(data.cFileName)) {
            files.push_back(data.cFileName);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    std::sort(files.begin(), files.end());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& file : files) {
        out.push_back(JoinPath(directory, file));
        if (out.size() > 5000) return;
    }
    for (const auto& dir : dirs) {
        CollectAudioFiles(JoinPath(directory, dir), out, depth + 1);
        if (out.size() > 5000) return;
    }
}

HWND OwnerWindow(App* app) {
    if (app->mainWnd && IsWindowVisible(app->mainWnd)) return app->mainWnd;
    if (app->floatWnd) return app->floatWnd;
    return app->mainWnd;
}
LRESULT CALLBACK ControllerWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }
    if (!app) return DefWindowProcW(hwnd, message, wParam, lParam);
    if (message == WM_TIMER && wParam == kTimerTick) {
        app->OnTimer();
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

App::~App() = default;
int App::Run(HINSTANCE hInstance, bool startFloat,
             const std::vector<std::wstring>& startupFiles) {
    instance = hInstance;
    oleInitialized_ = SUCCEEDED(OleInitialize(nullptr));
    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES |
                   ICC_WIN95_CLASSES;
    InitCommonControlsEx(&common);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        MessageBoxW(nullptr, L"无法初始化图形组件。", kAppName, MB_ICONERROR);
        return 1;
    }

    settings.Load();
    volume = settings.Data().volume;
    artwork.Initialize(instance);

    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
        LOGFONTW titleLog = metrics.lfMessageFont;
        titleLog.lfHeight = -20;
        titleLog.lfWeight = FW_SEMIBOLD;
        titleFont = CreateFontIndirectW(&titleLog);
    }
    if (!uiFont) {
        uiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    if (!titleFont) titleFont = uiFont;

    WNDCLASSEXW controllerClass{};
    controllerClass.cbSize = sizeof(controllerClass);
    controllerClass.lpfnWndProc = ControllerWindowProc;
    controllerClass.hInstance = instance;
    controllerClass.lpszClassName = kControllerClass;
    RegisterClassExW(&controllerClass);
    controllerWnd = CreateWindowExW(0, kControllerClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    instance, this);
    if (!controllerWnd) {
        MessageBoxW(nullptr, L"无法创建应用消息窗口。", kAppName, MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        if (oleInitialized_) OleUninitialize();
        return 1;
    }
    audio.Initialize(controllerWnd);
    audio.SetVolume(volume);

    if (startFloat) {
        CreateFloatingWindow();
        if (floatWnd) {
            ShowWindow(floatWnd, SW_SHOWNOACTIVATE);
            SetWindowPos(floatWnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    } else {
        CreateMainWindow();
        ShowMainWindow();
    }

    int startupIndex = -1;
    if (!startupFiles.empty()) {
        size_t before = playlist.size();
        AddFiles(startupFiles, false);
        if (playlist.size() > before) startupIndex = static_cast<int>(before);
    }
    if (startupIndex >= 0 && static_cast<size_t>(startupIndex) < playlist.size()) {
        PlayIndex(startupIndex, true);
    }
    AutoScanWorkingDirectory(false);
    HACCEL accelerators = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDA_MAIN));
    SetTimer(controllerWnd, kTimerTick, 500, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        HWND acceleratorWindow = mainWnd ? mainWnd : floatWnd;
        if (!accelerators || !acceleratorWindow ||
            !TranslateAcceleratorW(acceleratorWindow, accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    KillTimer(controllerWnd, kTimerTick);
    SaveSettings();
    audio.Shutdown();
    if (floatWnd) {
        DestroyWindow(floatWnd);
        floatWnd = nullptr;
    }
    if (mainWnd) {
        DestroyWindow(mainWnd);
        mainWnd = nullptr;
    }
    if (controllerWnd) {
        DestroyWindow(controllerWnd);
        controllerWnd = nullptr;
    }
    currentArtwork.reset();
    ReleaseCoverBitmaps();
    artwork.Shutdown();
    if (accelerators) DestroyAcceleratorTable(accelerators);
    if (uiFont && uiFont != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(uiFont);
    if (titleFont && titleFont != uiFont && titleFont != GetStockObject(DEFAULT_GUI_FONT))
        DeleteObject(titleFont);
    uiFont = nullptr;
    titleFont = nullptr;
    UnregisterClassW(kFloatWindowClass, instance);
    UnregisterClassW(kMainWindowClass, instance);
    UnregisterClassW(kControllerClass, instance);
    if (gdiplusToken_) Gdiplus::GdiplusShutdown(gdiplusToken_);
    if (oleInitialized_) {
        OleUninitialize();
        oleInitialized_ = false;
    }
    return 0;
}

void App::RequestQuit() {
    if (quitting_) return;
    quitting_ = true;
    KillTimer(controllerWnd, kTimerTick);
    SaveSettings();
    audio.Close();
    if (floatWnd) {
        DestroyWindow(floatWnd);
        floatWnd = nullptr;
    }
    if (mainWnd) {
        DestroyWindow(mainWnd);
        mainWnd = nullptr;
    }
    PostQuitMessage(0);
}

void App::OnTimer() {
    if (quitting_ || !audio.IsLoaded()) return;
    if (audio.ConsumeEnded()) {
        if (!lyricMakerOpen && !playlist.empty()) PlayNext(false);
        return;
    }
    if (userSeeking) return;
    position = audio.Position();
    if (duration <= 0.0) duration = audio.Duration();
    int second = static_cast<int>(position);
    if (second != lastPositionSec) {
        lastPositionSec = second;
        RefreshTransport();
    }
    int lyricIndex = lyrics.IndexAt(position);
    if (lyricIndex != lastLyricIndex) {
        lastLyricIndex = lyricIndex;
        RefreshLyricSelection();
        UpdateFloatingWindow(false);
    }
}

void App::SaveWindowPlacement() {
    if (!mainWnd || !IsWindow(mainWnd)) return;
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(mainWnd, &placement)) return;
    RECT rect = placement.rcNormalPosition;
    if (rect.right - rect.left < 400 || rect.bottom - rect.top < 300) return;
    settings.Data().mainRect = rect;
    settings.Data().hasMainRect = true;
}

void App::CommitWindowPlacement() {
    SaveWindowPlacement();
    settings.Data().volume = volume;
    settings.Save();
}

void App::SaveSettings() {
    SaveWindowPlacement();
    settings.Data().volume = volume;
    settings.Save();
}

void App::CreateMainWindow() {
    if (mainWnd && IsWindow(mainWnd)) return;
    mainWnd = MainWindowCreate(this);
}

void App::ShowMainWindow() {
    CreateMainWindow();
    if (!mainWnd) return;
    ShowWindow(mainWnd, SW_SHOW);
    ShowWindow(mainWnd, SW_RESTORE);
    SetForegroundWindow(mainWnd);
    LayoutMainWindow();
}

void App::HideMainWindow() {
    if (mainWnd) ShowWindow(mainWnd, SW_HIDE);
}

void App::ShowFloatingWindow() {
    CreateFloatingWindow();
    if (!floatWnd) return;
    ShowWindow(floatWnd, SW_SHOWNOACTIVATE);
    SetWindowPos(floatWnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    LayoutFloatingWindow();
    UpdateFloatingWindow(true);
}

void App::HideFloatingWindow() {
    if (floatWnd) ShowWindow(floatWnd, SW_HIDE);
}

void App::ToggleFloatingFromMain() {
    CreateFloatingWindow();
    if (!floatWnd) return;
    HideMainWindow();
    ShowFloatingWindow();
}

void App::ReturnToMainFromFloat() {
    HideFloatingWindow();
    ShowMainWindow();
    SetForegroundWindow(mainWnd);
}

void App::AddFiles(const std::vector<std::wstring>& paths, bool showResult) {
    if (paths.empty()) return;
    std::vector<std::wstring> expanded;
    for (const auto& path : paths) {
        if (DirectoryExists(path)) CollectAudioFiles(path, expanded);
        else if (FileExists(path) && IsSupportedAudio(path)) expanded.push_back(path);
    }
    if (expanded.empty()) {
        if (showResult) MessageBoxW(OwnerWindow(this), L"没有找到可播放的音频文件。",
                                    L"添加音乐", MB_ICONINFORMATION | MB_OK);
        return;
    }
    std::set<std::wstring> known;
    for (const auto& track : playlist) known.insert(ToLower(track.path));
    HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    int added = 0;
    for (const auto& original : expanded) {
        std::wstring path = GetFullPath(original);
        std::wstring key = ToLower(path);
        if (known.count(key)) continue;
        Track track;
        track.path = path;
        track.info = tagReader.Read(path, false, false);
        playlist.push_back(std::move(track));
        known.insert(key);
        ++added;
        if (playlist.size() >= 5000) break;
    }
    SetCursor(oldCursor);
    if (!expanded.empty()) settings.Data().lastDir = DirNameOf(GetFullPath(expanded.front()));
    if (selectedIndex < 0 && !playlist.empty()) selectedIndex = 0;
    RefreshPlaylist();
    UpdateStatus();
    if (showResult) {
        statusText = added > 0 ? (L"已添加 " + NumberText(added) + L" 首音乐") : L"这些歌曲已在播放列表中";
        UpdateStatus();
    }
    RefreshTransport();
}

void App::AddFilesFromDialog() {
    std::vector<wchar_t> buffer(65536, 0);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = OwnerWindow(this);
    dialog.lpstrFilter =
        L"音频文件\0*.mp3;*.flac;*.wav;*.m4a;*.m4b;*.aac;*.ogg;*.oga;*.opus;*.wma;*.ape;*.aiff;*.aif;*.wv\0"
        L"MP3\0*.mp3\0FLAC\0*.flac\0WAV\0*.wav\0所有文件\0*.*\0\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = L"添加音乐";
    dialog.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!settings.Data().lastDir.empty() && DirectoryExists(settings.Data().lastDir))
        dialog.lpstrInitialDir = settings.Data().lastDir.c_str();
    if (!GetOpenFileNameW(&dialog)) return;

    std::vector<std::wstring> files;
    std::wstring first = buffer.data();
    size_t firstLen = first.size();
    if (firstLen + 1 < buffer.size() && buffer[firstLen + 1] != 0) {
        std::wstring directory = first;
        size_t pos = firstLen + 1;
        while (pos < buffer.size() && buffer[pos] != 0) {
            std::wstring name = &buffer[pos];
            files.push_back(JoinPath(directory, name));
            pos += name.size() + 1;
        }
    } else {
        files.push_back(first);
    }
    AddFiles(files, true);
}

void App::AddFolderFromDialog() {
    BROWSEINFOW browse{};
    browse.hwndOwner = OwnerWindow(this);
    browse.lpszTitle = L"选择包含音乐的文件夹";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_DONTGOBELOWDOMAIN;
    LPITEMIDLIST item = SHBrowseForFolderW(&browse);
    if (!item) return;
    wchar_t path[MAX_PATH] = {};
    bool ok = SHGetPathFromIDListW(item, path) != FALSE;
    CoTaskMemFree(item);
    if (!ok) return;
    AddFiles({path}, true);
}

void App::AutoScanWorkingDirectory(bool userInitiated) {
    if (!mainWnd && !floatWnd) return;
    std::wstring appDir = GetExeDir();
    if (appDir.empty()) return;
    std::vector<std::wstring> found;
    CollectAudioFiles(appDir, found);
    if (found.empty()) {
        if (userInitiated) {
            MessageBoxW(OwnerWindow(this),
                        L"软件所在目录没有找到音频文件。\n把 MP3 等音频放在 MAKASOUNDER.exe 旁边即可自动导入。",
                        L"扫描软件目录", MB_ICONINFORMATION | MB_OK);
        }
        return;
    }
    std::set<std::wstring> known;
    for (const auto& track : playlist) known.insert(ToLower(track.path));
    std::vector<std::wstring> fresh;
    for (const auto& path : found) {
        std::wstring full = GetFullPath(path);
        std::wstring key = ToLower(full);
        if (known.insert(key).second) fresh.push_back(full);
    }
    if (fresh.empty()) {
        if (userInitiated) {
            MessageBoxW(OwnerWindow(this), L"软件目录中的音频都已经在播放列表中。",
                        L"扫描软件目录", MB_ICONINFORMATION | MB_OK);
        }
        return;
    }

    if (fresh.size() > kAutoScanPromptLimit) {
        std::wstring message = L"在软件所在目录找到 " + NumberText(static_cast<int>(fresh.size())) +
                               L" 首尚未导入的音频文件。\n\n全部导入播放列表吗？\n（文件很多，导入时可能需要一点时间）";
        if (mainWnd) UpdateWindow(mainWnd);
        if (MessageBoxW(OwnerWindow(this), message.c_str(), L"自动导入音乐",
                        MB_YESNO | MB_ICONQUESTION) != IDYES) {
            statusText = L"已跳过软件目录自动导入";
            UpdateStatus();
            return;
        }
    }

    int before = static_cast<int>(playlist.size());
    AddFiles(fresh, false);
    int added = static_cast<int>(playlist.size()) - before;
    if (added > 0) {
        statusText = L"已从软件目录导入 " + NumberText(added) + L" 首音乐";
        if (selectedIndex < 0) selectedIndex = 0;
        RefreshPlaylist();
    } else {
        statusText = L"软件目录没有新的音频文件";
    }
    UpdateStatus();
}

void App::RemoveTrack(int index) {
    if (index < 0 || static_cast<size_t>(index) >= playlist.size()) return;
    bool wasCurrent = index == currentIndex;
    playlist.erase(playlist.begin() + index);
    if (wasCurrent) {
        audio.Close();
        lyrics.Clear();
        currentArtwork.reset();
        currentIndex = -1;
        position = 0.0;
        duration = 0.0;
        lastLyricIndex = -2;
        if (!playlist.empty()) {
            int next = std::min(index, static_cast<int>(playlist.size()) - 1);
            selectedIndex = next;
            PlayIndex(next, true);
        } else {
            selectedIndex = -1;
            RefreshAll(true);
            UpdateWindowTitle();
        }
    } else {
        if (currentIndex > index) --currentIndex;
        if (selectedIndex > index) --selectedIndex;
        if (selectedIndex >= static_cast<int>(playlist.size()))
            selectedIndex = static_cast<int>(playlist.size()) - 1;
        RefreshPlaylist();
        RefreshTransport();
    }
    UpdateStatus();
}

void App::ClearPlaylist() {
    audio.Close();
    playlist.clear();
    currentIndex = -1;
    selectedIndex = -1;
    currentArtwork.reset();
    lyrics.Clear();
    position = 0.0;
    duration = 0.0;
    lastLyricIndex = -2;
    lastPositionSec = -1;
    lastDurationSec = -1;
    RefreshAll(true);
    UpdateWindowTitle();
    UpdateStatus();
}

void App::PlaySelected() {
    if (playlist.empty()) {
        AddFilesFromDialog();
        return;
    }
    int index = selectedIndex;
    if (index < 0 || static_cast<size_t>(index) >= playlist.size()) index = 0;
    PlayIndex(index, true);
}

void App::PlayIndex(int index, bool autoplay) {
    if (index < 0 || static_cast<size_t>(index) >= playlist.size()) return;
    currentIndex = index;
    selectedIndex = index;
    Track& track = playlist[static_cast<size_t>(index)];
    track.info = tagReader.Read(track.path, true, true);
    currentArtwork = artwork.LoadFor(track.path, track.info.artwork);
    lyrics.LoadForTrack(track.path, track.info.title, track.info.embeddedLyrics);

    std::wstring error;
    bool opened = audio.Open(track.path, error);
    if (opened) {
        audio.SetVolume(volume);
        duration = audio.Duration();
        if (duration <= 0.0) duration = track.info.durationSec;
        position = 0.0;
        if (autoplay) {
            if (!audio.Play()) {
                error = audio.LastError();
                opened = false;
            }
        }
    } else {
        duration = track.info.durationSec;
        position = 0.0;
    }
    lastLyricIndex = -2;
    lastPositionSec = -1;
    lastDurationSec = static_cast<int>(duration);
    track.info.durationSec = duration;
    RefreshAll(true);
    UpdateWindowTitle();
    if (opened) {
        statusText = L"正在播放：" + CurrentTitle();
    } else {
        statusText = L"无法播放：" + CurrentTitle() + L" · " + error;
        if (!mainWnd || !IsWindowVisible(mainWnd)) {
            MessageBoxW(OwnerWindow(this), error.c_str(), L"MAKASOUNDER 无法播放", MB_ICONERROR | MB_OK);
        }
    }
    if (opened && !lyrics.HasAny()) {
        statusText += L" · 未找到歌词";
    }
    UpdateStatus();
}

void App::TogglePlayback() {
    if (playlist.empty()) {
        AddFilesFromDialog();
        return;
    }
    if (currentIndex < 0) {
        PlaySelected();
        return;
    }
    if (!audio.IsLoaded()) {
        PlayIndex(currentIndex, true);
        return;
    }
    if (duration > 0.0 && position >= duration - 0.25 && !audio.IsPlaying()) {
        PlayIndex(currentIndex, true);
        return;
    }
    if (!audio.TogglePlayPause()) {
        statusText = L"播放操作失败：" + audio.LastError();
        UpdateStatus();
    } else {
        statusText = audio.IsPlaying() ? L"正在播放：" + CurrentTitle() : L"已暂停：" + CurrentTitle();
        UpdateStatus();
    }
    RefreshTransport();
    UpdateFloatingWindow(false);
}

void App::PlayNext(bool manual) {
    if (playlist.empty()) return;
    int next = currentIndex + 1;
    if (next >= static_cast<int>(playlist.size())) next = 0;
    PlayIndex(next, true);
    (void)manual;
}

void App::PlayPrev() {
    if (playlist.empty()) return;
    if (position > 3.0 && audio.IsLoaded()) {
        SeekTo(0.0);
        return;
    }
    int prev = currentIndex - 1;
    if (prev < 0) prev = static_cast<int>(playlist.size()) - 1;
    PlayIndex(prev, true);
}

void App::SeekTo(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    if (duration > 0.0 && seconds > duration) seconds = duration;
    if (audio.IsLoaded()) audio.Seek(seconds);
    position = seconds;
    lastPositionSec = static_cast<int>(position);
    RefreshTransport();
    UpdateFloatingWindow(false);
}

void App::SetVolume(int percent) {
    volume = ClampValue(percent, 0, 100);
    audio.SetVolume(volume);
    settings.Data().volume = volume;
    if (volumeLabel) SetWindowTextW(volumeLabel, (L"音量 " + NumberText(volume)).c_str());
    if (volumeBar && GetFocus() != volumeBar) {
        SendMessageW(volumeBar, TBM_SETPOS, TRUE, volume);
    }
}

Track* App::CurrentTrack() {
    if (currentIndex < 0 || static_cast<size_t>(currentIndex) >= playlist.size()) return nullptr;
    return &playlist[static_cast<size_t>(currentIndex)];
}

const Track* App::CurrentTrack() const {
    if (currentIndex < 0 || static_cast<size_t>(currentIndex) >= playlist.size()) return nullptr;
    return &playlist[static_cast<size_t>(currentIndex)];
}

std::wstring App::CurrentTitle() const {
    const Track* track = CurrentTrack();
    if (!track) return L"未选择歌曲";
    return track->info.title.empty() ? StemOf(track->path) : track->info.title;
}

std::wstring App::CurrentArtist() const {
    const Track* track = CurrentTrack();
    if (!track || track->info.artist.empty()) return L"未知艺术家";
    return track->info.artist;
}

std::wstring App::CurrentAlbum() const {
    const Track* track = CurrentTrack();
    return track ? track->info.album : L"";
}

HBITMAP App::CreateCoverBitmap(int size) const {
    if (size <= 0) return nullptr;
    Gdiplus::Bitmap* source = currentArtwork ? currentArtwork.get() : nullptr;
    if (!source) {
        // The placeholder is owned by the service; keep the const method safe.
        auto placeholder = const_cast<ArtworkService&>(artwork).Placeholder();
        source = placeholder.get();
    }
    if (!source) return nullptr;
    Gdiplus::Bitmap canvas(size, size, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&canvas);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.Clear(Gdiplus::Color(255, 30, 31, 35));
    float srcW = static_cast<float>(source->GetWidth());
    float srcH = static_cast<float>(source->GetHeight());
    float scale = std::max(static_cast<float>(size) / srcW, static_cast<float>(size) / srcH);
    float drawW = srcW * scale;
    float drawH = srcH * scale;
    graphics.DrawImage(source, Gdiplus::RectF((size - drawW) * 0.5f, (size - drawH) * 0.5f,
                                              drawW, drawH),
                       0, 0, srcW, srcH, Gdiplus::UnitPixel);
    HBITMAP bitmap = nullptr;
    if (canvas.GetHBITMAP(Gdiplus::Color(255, 30, 31, 35), &bitmap) != Gdiplus::Ok) return nullptr;
    return bitmap;
}

void App::ReleaseCoverBitmaps() {
    if (coverBitmap) {
        DeleteObject(coverBitmap);
        coverBitmap = nullptr;
    }
    if (floatCoverBitmap) {
        DeleteObject(floatCoverBitmap);
        floatCoverBitmap = nullptr;
    }
}

void App::OpenSettings(HWND owner) {
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), owner, SettingsDialogProc,
                    reinterpret_cast<LPARAM>(this));
}

void App::OpenLyricMaker(HWND owner) {
    if (!CurrentTrack()) {
        MessageBoxW(owner, L"请先选择一首歌曲。", L"歌词制作器", MB_ICONINFORMATION | MB_OK);
        return;
    }
    makerLines.clear();
    makerSelected = -1;
    makerSeeking = false;
    for (const auto& line : lyrics.Lines()) makerLines.push_back({line.timeMs, line.text});
    lyricMakerOpen = true;
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MAKER), owner, MakerDialogProc,
                    reinterpret_cast<LPARAM>(this));
    lyricMakerOpen = false;
}

void App::ApplyAssociation(bool enable, bool floatMode, HWND owner) {
    std::wstring error;
    bool ok = enable ? RegisterAssociation(floatMode, error) : RemoveAssociation(error);
    if (!ok) {
        MessageBoxW(owner, error.c_str(), L"文件关联", MB_ICONERROR | MB_OK);
        return;
    }
    MessageBeep(MB_OK);
    statusText = enable ? L"MP3 文件关联已更新" : L"已移除 MAKASOUNDER 文件关联";
    UpdateStatus();
}

void App::ShowAbout(HWND owner) {
    std::wstring message = L"MAKASOUNDER ";
    message += kAppVersion;
    message += L"\n离线音乐播放器\n\n";
    message += L"引擎：";
    message += audio.BackendName();
    message += L"\n设置目录：";
    message += GetAppDataDir();
    message += L"\n\n不联网、不上传任何数据。";
    MessageBoxW(owner, message.c_str(), L"关于 MAKASOUNDER", MB_ICONINFORMATION | MB_OK);
}

}  // namespace mk
