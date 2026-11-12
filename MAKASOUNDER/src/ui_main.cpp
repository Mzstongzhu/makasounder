#include "app.h"

#include "common.h"
#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace mk {

namespace {

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

constexpr int kMainMinWidth = 780;
constexpr int kMainMinHeight = 600;

std::wstring NumberText(int value) {
    wchar_t buffer[32];
    _snwprintf(buffer, 32, L"%d", value);
    buffer[31] = 0;
    return buffer;
}

void SetFont(HWND control, HFONT font) {
    if (control && font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HWND CreateControl(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style,
                   int id, HFONT font) {
    HWND control = CreateWindowExW(
        0, className, text, style | WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SetFont(control, font);
    return control;
}

void AppendMenuText(HMENU menu, UINT flags, UINT_PTR id, const wchar_t* text) {
    AppendMenuW(menu, flags, id, text);
}

HMENU BuildMainMenu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuText(file, MF_STRING, IDM_FILE_ADD, L"添加文件...\tCtrl+O");
    AppendMenuText(file, MF_STRING, IDM_FILE_ADD_FOLDER, L"添加文件夹...");
    AppendMenuText(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuText(file, MF_STRING, IDM_FILE_CLEAR, L"清空播放列表");
    AppendMenuText(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuText(file, MF_STRING, IDM_FILE_EXIT, L"退出");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"文件(&F)");

    HMENU play = CreatePopupMenu();
    AppendMenuText(play, MF_STRING, IDM_PLAY_TOGGLE, L"播放 / 暂停\tSpace");
    AppendMenuText(play, MF_STRING, IDM_PLAY_PREV, L"上一首\tCtrl+Left");
    AppendMenuText(play, MF_STRING, IDM_PLAY_NEXT, L"下一首\tCtrl+Right");
    AppendMenuText(play, MF_SEPARATOR, 0, nullptr);
    AppendMenuText(play, MF_STRING, IDM_PLAY_FLOAT, L"进入悬浮窗");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(play), L"播放(&P)");

    HMENU tools = CreatePopupMenu();
    AppendMenuText(tools, MF_STRING, IDM_TOOLS_MAKER, L"歌词制作器...");
    AppendMenuText(tools, MF_STRING, IDM_TOOLS_SCAN, L"扫描软件目录音频");
    AppendMenuText(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuText(tools, MF_STRING, IDM_TOOLS_SETTINGS, L"设置...");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"工具(&T)");

    HMENU help = CreatePopupMenu();
    AppendMenuText(help, MF_STRING, IDM_HELP_ABOUT, L"关于 MAKASOUNDER");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"帮助(&H)");
    return menu;
}

void LayoutMain(App* app) {
    if (!app->mainWnd) return;
    RECT client{};
    GetClientRect(app->mainWnd, &client);
    int width = client.right;
    int height = client.bottom;

    SendMessageW(app->statusBar, WM_SIZE, 0, 0);
    RECT statusRect{};
    GetWindowRect(app->statusBar, &statusRect);
    int statusHeight = statusRect.bottom - statusRect.top;
    int parts[3] = {std::max(120, width - 390), std::max(180, width - 190), -1};
    SendMessageW(app->statusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

    int margin = 10;
    int toolbarY = 10;
    int toolbarH = 30;
    int contentTop = 50;
    int contentBottom = height - statusHeight - 8;
    if (contentBottom < contentTop + 100) contentBottom = contentTop + 100;
    int contentHeight = contentBottom - contentTop;

    int settingsX = width - margin - 72;
    int floatX = settingsX - 8 - 92;
    MoveWindow(app->settingsButton, settingsX, toolbarY, 72, toolbarH, TRUE);
    MoveWindow(app->floatButton, floatX, toolbarY, 92, toolbarH, TRUE);
    MoveWindow(GetDlgItem(app->mainWnd, IDC_MAIN_ADD), margin, toolbarY, 100, toolbarH, TRUE);
    MoveWindow(GetDlgItem(app->mainWnd, IDC_MAIN_FOLDER), margin + 108, toolbarY, 112, toolbarH,
               TRUE);
    MoveWindow(GetDlgItem(app->mainWnd, IDC_MAIN_CLEAR), margin + 228, toolbarY, 72, toolbarH,
               TRUE);

    int leftW = 286;
    int leftX = margin;
    MoveWindow(GetDlgItem(app->mainWnd, 9001), leftX, contentTop, leftW, contentHeight, TRUE);

    int cover = 180;
    int coverX = leftX + (leftW - cover) / 2;
    int coverY = contentTop + 24;
    MoveWindow(app->coverStatic, coverX, coverY, cover, cover, TRUE);

    int textX = leftX + 14;
    int textW = leftW - 28;
    int y = coverY + cover + 12;
    MoveWindow(app->titleLabel, textX, y, textW, 28, TRUE);
    y += 30;
    MoveWindow(app->artistLabel, textX, y, textW, 20, TRUE);
    y += 22;
    MoveWindow(app->albumLabel, textX, y, textW, 20, TRUE);
    y += 24;
    MoveWindow(app->seekBar, textX, y, textW, 30, TRUE);
    y += 32;
    MoveWindow(app->timeLabel, textX, y, textW, 18, TRUE);
    y += 24;
    int gap = 8;
    int buttonW = (textW - gap * 2) / 3;
    MoveWindow(app->prevButton, textX, y, buttonW, 34, TRUE);
    MoveWindow(app->playButton, textX + buttonW + gap, y, buttonW, 34, TRUE);
    MoveWindow(app->nextButton, textX + (buttonW + gap) * 2, y, buttonW, 34, TRUE);
    y += 46;
    MoveWindow(app->volumeLabel, textX, y, textW, 18, TRUE);
    y += 22;
    MoveWindow(app->volumeBar, textX, y, textW, 28, TRUE);

    // Keep the now-playing block inside the left group at small heights.
    if (y + 34 > contentBottom) {
        // The buttons remain usable; scrollbars are not added to this simple panel.
    }

    int rightX = leftX + leftW + 10;
    int rightW = width - rightX - margin;
    if (rightW < 300) rightW = 300;
    MoveWindow(GetDlgItem(app->mainWnd, 9002), rightX, contentTop, rightW, 24, TRUE);
    int listTop = contentTop + 28;
    int listH = static_cast<int>(contentHeight * 0.56);
    if (listH < 150) listH = 150;
    int lyricsLabelY = listTop + listH + 8;
    int lyricsTop = lyricsLabelY + 24;
    int lyricsH = contentBottom - lyricsTop;
    if (lyricsH < 100) {
        lyricsH = 100;
        listH = std::max(100, contentBottom - listTop - 24 - 100 - 8);
        lyricsLabelY = listTop + listH + 8;
        lyricsTop = lyricsLabelY + 24;
    }
    MoveWindow(app->listView, rightX, listTop, rightW, listH, TRUE);
    MoveWindow(GetDlgItem(app->mainWnd, 9003), rightX, lyricsLabelY, 50, 20, TRUE);
    MoveWindow(app->lyricsSourceLabel, rightX + 54, lyricsLabelY, rightW - 150, 20, TRUE);
    MoveWindow(GetDlgItem(app->mainWnd, IDC_MAKE_LYRICS), rightX + rightW - 88,
               lyricsLabelY - 2, 88, 24, TRUE);
    MoveWindow(app->lyricsList, rightX, lyricsTop, rightW, lyricsH, TRUE);

    HWND list = app->listView;
    if (list) {
        int indexWidth = 44;
        int artistWidth = std::min(190, std::max(110, rightW / 4));
        int durationWidth = 62;
        int titleWidth = std::max(100, rightW - indexWidth - artistWidth - durationWidth - 24);
        ListView_SetColumnWidth(list, 0, indexWidth);
        ListView_SetColumnWidth(list, 1, titleWidth);
        ListView_SetColumnWidth(list, 2, artistWidth);
        ListView_SetColumnWidth(list, 3, durationWidth);
    }
    InvalidateRect(app->mainWnd, nullptr, FALSE);
}

void FillListItem(App* app, NMLVDISPINFOW* info) {
    if (!info) return;
    int index = info->item.iItem;
    if (index < 0 || static_cast<size_t>(index) >= app->playlist.size()) return;
    const Track& track = app->playlist[static_cast<size_t>(index)];
    if (info->item.mask & LVIF_TEXT) {
        switch (info->item.iSubItem) {
            case 0:
                lstrcpynW(info->item.pszText, NumberText(index + 1).c_str(), info->item.cchTextMax);
                break;
            case 1:
                lstrcpynW(info->item.pszText,
                          (track.info.title.empty() ? StemOf(track.path) : track.info.title).c_str(),
                          info->item.cchTextMax);
                break;
            case 2:
                lstrcpynW(info->item.pszText,
                          (track.info.artist.empty() ? L"未知艺术家" : track.info.artist).c_str(),
                          info->item.cchTextMax);
                break;
            case 3:
                lstrcpynW(info->item.pszText, FormatTime(track.info.durationSec).c_str(),
                          info->item.cchTextMax);
                break;
            default: break;
        }
    }
}

void ShowTrackContextMenu(App* app, int index, POINT screen) {
    if (index < 0 || static_cast<size_t>(index) >= app->playlist.size()) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, 1, L"播放");
    AppendMenuW(menu, MF_STRING, 2, L"从列表中移除");
    AppendMenuW(menu, MF_STRING, 3, L"在资源管理器中显示");
    SetForegroundWindow(app->mainWnd);
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0,
                                 app->mainWnd, nullptr);
    DestroyMenu(menu);
    switch (command) {
        case 1: app->PlayIndex(index, true); break;
        case 2: app->RemoveTrack(index); break;
        case 3: {
            std::wstring args = L"/select,\"" + app->playlist[static_cast<size_t>(index)].path + L"\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOW);
            break;
        }
        default: break;
    }
    PostMessageW(app->mainWnd, WM_NULL, 0, 0);
}

void UpdateAssociationLabel(HWND dialog, App* app) {
    AssociationStatus status = QueryAssociation();
    std::wstring text = status.registered ? L"当前状态：已关联 .mp3（当前用户）"
                                          : L"当前状态：未关联 .mp3";
    if (status.registered && status.floatMode) text += L"，双击直达悬浮窗";
    SetDlgItemTextW(dialog, IDC_SET_STATUS, text.c_str());
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }
    if (!app) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = kMainMinWidth;
            info->ptMinTrackSize.y = kMainMinHeight;
            return 0;
        }
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) LayoutMain(app);
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_3DFACE));
        }
        case WM_HSCROLL: {
            if (app->updatingControls) return 0;
            HWND control = reinterpret_cast<HWND>(lParam);
            int code = LOWORD(wParam);
            if (control == app->seekBar) {
                int value = static_cast<int>(SendMessageW(app->seekBar, TBM_GETPOS, 0, 0));
                if (app->duration > 0.0) {
                    double target = app->duration * value / 1000.0;
                    if (code == TB_THUMBTRACK) {
                        app->userSeeking = true;
                        std::wstring text = FormatTime(target) + L" / " + FormatTime(app->duration);
                        SetWindowTextW(app->timeLabel, text.c_str());
                    } else if (code == TB_ENDTRACK || code == TB_THUMBPOSITION ||
                               code == TB_LINEUP || code == TB_LINEDOWN || code == TB_PAGEUP ||
                               code == TB_PAGEDOWN || code == TB_TOP || code == TB_BOTTOM) {
                        app->userSeeking = false;
                        app->SeekTo(target);
                    }
                }
            } else if (control == app->volumeBar) {
                int value = static_cast<int>(SendMessageW(app->volumeBar, TBM_GETPOS, 0, 0));
                app->SetVolume(value);
                if (code == TB_ENDTRACK || code == TB_THUMBPOSITION) app->SaveSettings();
            }
            return 0;
        }
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header->hwndFrom == app->listView) {
                if (header->code == LVN_GETDISPINFOW) {
                    FillListItem(app, reinterpret_cast<NMLVDISPINFOW*>(lParam));
                    return 0;
                }
                if (header->code == LVN_ITEMCHANGED && !app->updatingControls) {
                    auto* changed = reinterpret_cast<NMLISTVIEW*>(lParam);
                    if ((changed->uNewState & LVIS_SELECTED) && changed->iItem >= 0) {
                        app->selectedIndex = changed->iItem;
                    }
                    return 0;
                }
                if (header->code == static_cast<UINT>(NM_DBLCLK)) {
                    auto* activate = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                    if (activate->iItem >= 0) app->PlayIndex(activate->iItem, true);
                    return 0;
                }
                if (header->code == static_cast<UINT>(NM_RCLICK)) {
                    auto* activate = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                    if (activate->iItem >= 0) {
                        app->selectedIndex = activate->iItem;
                        POINT screen{};
                        GetCursorPos(&screen);
                        ShowTrackContextMenu(app, activate->iItem, screen);
                    }
                    return 0;
                }
                if (header->code == LVN_KEYDOWN) {
                    auto* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
                    if (key->wVKey == VK_RETURN) {
                        app->PlaySelected();
                        return 0;
                    }
                    if (key->wVKey == VK_DELETE) {
                        if (app->selectedIndex >= 0) app->RemoveTrack(app->selectedIndex);
                        return 0;
                    }
                }
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_MAIN_ADD:
                case IDM_FILE_ADD:
                    app->AddFilesFromDialog();
                    return 0;
                case IDC_MAIN_FOLDER:
                case IDM_FILE_ADD_FOLDER:
                    app->AddFolderFromDialog();
                    return 0;
                case IDC_MAIN_CLEAR:
                case IDM_FILE_CLEAR:
                    if (!app->playlist.empty() &&
                        MessageBoxW(app->mainWnd, L"确定清空当前播放列表吗？", L"清空播放列表",
                                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        app->ClearPlaylist();
                    }
                    return 0;
                case IDC_MAIN_FLOAT:
                case IDM_PLAY_FLOAT:
                    app->ToggleFloatingFromMain();
                    return 0;
                case IDC_MAIN_SETTINGS:
                case IDM_TOOLS_SETTINGS:
                    app->OpenSettings(app->mainWnd);
                    return 0;
                case IDC_PREV:
                case IDM_PLAY_PREV:
                    app->PlayPrev();
                    return 0;
                case IDC_PLAY:
                case IDM_PLAY_TOGGLE:
                    app->TogglePlayback();
                    return 0;
                case IDC_NEXT:
                case IDM_PLAY_NEXT:
                    app->PlayNext(true);
                    return 0;
                case IDC_MAKE_LYRICS:
                case IDM_TOOLS_MAKER:
                    app->OpenLyricMaker(app->mainWnd);
                    return 0;
                case IDM_TOOLS_SCAN:
                    app->AutoScanWorkingDirectory(true);
                    return 0;
                case IDM_HELP_ABOUT:
                    app->ShowAbout(app->mainWnd);
                    return 0;
                case IDM_FILE_EXIT:
                    app->RequestQuit();
                    return 0;
                case IDM_VOLUME_UP:
                    app->SetVolume(app->volume + 5);
                    app->SaveSettings();
                    return 0;
                case IDM_VOLUME_DOWN:
                    app->SetVolume(app->volume - 5);
                    app->SaveSettings();
                    return 0;
                case IDM_MEDIA_PLAY_PAUSE:
                    if (!app->playlist.empty()) {
                        if (app->currentIndex < 0) app->PlaySelected();
                        else app->TogglePlayback();
                    }
                    return 0;
                case IDM_MEDIA_PREV:
                    if (!app->playlist.empty()) app->PlayPrev();
                    return 0;
                case IDM_MEDIA_NEXT:
                    if (!app->playlist.empty()) app->PlayNext(true);
                    return 0;
                case IDM_MEDIA_STOP:
                    if (app->audio.IsPlaying()) app->TogglePlayback();
                    return 0;
                default:
                    break;
            }
            break;
        }
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::wstring> files;
            for (UINT i = 0; i < count; ++i) {
                UINT length = DragQueryFileW(drop, i, nullptr, 0);
                std::vector<wchar_t> buffer(length + 1, L'\0');
                DragQueryFileW(drop, i, buffer.data(), length + 1);
                files.emplace_back(buffer.data());
            }
            DragFinish(drop);
            app->AddFiles(files, true);
            SetForegroundWindow(hwnd);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (HandleMediaKeys(app, message, wParam, lParam)) return 0;
            if (wParam == VK_DELETE && app->selectedIndex >= 0) {
                app->RemoveTrack(app->selectedIndex);
                return 0;
            }
            break;
        case WM_APPCOMMAND:
            if (HandleMediaKeys(app, message, wParam, lParam)) return TRUE;
            break;
        case WM_CLOSE:
            app->RequestQuit();
            return 0;
        case WM_DESTROY:
            app->mainWnd = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

bool HandleMediaKeys(App* app, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!app) return false;
    auto togglePlay = [&] {
        if (app->playlist.empty()) return;
        if (app->currentIndex < 0) app->PlaySelected();
        else app->TogglePlayback();
    };
    auto next = [&] {
        if (app->lyricMakerOpen) return;
        if (!app->playlist.empty()) app->PlayNext(true);
    };
    auto prev = [&] {
        if (app->lyricMakerOpen) return;
        if (!app->playlist.empty()) app->PlayPrev();
    };
    if (message == WM_APPCOMMAND) {
        // MinGW's winuser.h is old and does not define these AppCommand values.
        enum {
            kAppMediaNext = 11,
            kAppMediaPrev = 12,
            kAppMediaStop = 13,
            kAppMediaPlayPause = 14,
            kAppMediaFastForward = 49,
            kAppMediaRewind = 50
        };
        int command = static_cast<short>(HIWORD(lParam));
        switch (command) {
            case kAppMediaPlayPause:
                togglePlay();
                return true;
            case kAppMediaNext:
                next();
                return true;
            case kAppMediaPrev:
                prev();
                return true;
            case kAppMediaStop:
                if (app->audio.IsPlaying()) app->TogglePlayback();
                return true;
            case kAppMediaFastForward:
                app->SeekTo(app->position + 5.0);
                return true;
            case kAppMediaRewind:
                app->SeekTo(app->position - 5.0);
                return true;
            default:
                return false;
        }
    }
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        switch (wParam) {
            case VK_MEDIA_PLAY_PAUSE:
                togglePlay();
                return true;
            case VK_MEDIA_NEXT_TRACK:
                next();
                return true;
            case VK_MEDIA_PREV_TRACK:
                prev();
                return true;
            case VK_MEDIA_STOP:
                if (app->audio.IsPlaying()) app->TogglePlayback();
                return true;
            default:
                break;
        }
    }
    return false;
}

HWND MainWindowCreate(App* app) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_DBLCLKS;
        windowClass.lpfnWndProc = MainWndProc;
        windowClass.hInstance = app->instance;
        windowClass.hIcon = LoadIconW(app->instance, MAKEINTRESOURCEW(IDI_APP));
        windowClass.hIconSm = LoadIconW(app->instance, MAKEINTRESOURCEW(IDI_APP));
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
        windowClass.lpszClassName = kMainWindowClass;
        if (!RegisterClassExW(&windowClass)) return nullptr;
        registered = true;
    }

    HMENU menu = BuildMainMenu();
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT windowRect{0, 0, 980, 640};
    AdjustWindowRectEx(&windowRect, style, TRUE, WS_EX_APPWINDOW);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    int x = 0;
    int y = 0;
    bool placed = false;
    HMONITOR primaryMonitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(primaryMonitor, &info)) work = info.rcWork;
    if (app->settings.Data().hasMainRect) {
        RECT saved = app->settings.Data().mainRect;
        int savedW = saved.right - saved.left;
        int savedH = saved.bottom - saved.top;
        if (savedW >= 500 && savedH >= 400) {
            width = std::max(static_cast<int>(kMainMinWidth), savedW);
            height = std::max(static_cast<int>(kMainMinHeight), savedH);
            x = saved.left;
            y = saved.top;
            placed = true;
            HMONITOR savedMonitor = MonitorFromRect(&saved, MONITOR_DEFAULTTONULL);
            if (savedMonitor && GetMonitorInfoW(savedMonitor, &info)) work = info.rcWork;
        }
    }
    if (!placed) {
        x = work.left + (work.right - work.left - width) / 2;
        y = work.top + (work.bottom - work.top - height) / 2;
    }
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;
    if (x + width > work.right) x = std::max(work.left, work.right - width);
    if (y + height > work.bottom) y = std::max(work.top, work.bottom - height);

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, kMainWindowClass, L"MAKASOUNDER", style, x, y,
                                width, height, nullptr, menu, app->instance, app);
    if (!hwnd) {
        DestroyMenu(menu);
        return nullptr;
    }
    app->mainWnd = hwnd;
    SetWindowTextW(hwnd, L"MAKASOUNDER");
    app->UpdateWindowTitle();
    DragAcceptFiles(hwnd, TRUE);


    app->settingsButton = CreateControl(hwnd, L"BUTTON", L"设置", WS_TABSTOP | BS_PUSHBUTTON,
                                        IDC_MAIN_SETTINGS, app->uiFont);
    app->floatButton = CreateControl(hwnd, L"BUTTON", L"悬浮窗", WS_TABSTOP | BS_PUSHBUTTON,
                                     IDC_MAIN_FLOAT, app->uiFont);
    CreateControl(hwnd, L"BUTTON", L"添加音乐", WS_TABSTOP | BS_PUSHBUTTON, IDC_MAIN_ADD,
                  app->uiFont);
    CreateControl(hwnd, L"BUTTON", L"添加文件夹", WS_TABSTOP | BS_PUSHBUTTON, IDC_MAIN_FOLDER,
                  app->uiFont);
    CreateControl(hwnd, L"BUTTON", L"清空", WS_TABSTOP | BS_PUSHBUTTON, IDC_MAIN_CLEAR,
                  app->uiFont);
    CreateControl(hwnd, L"BUTTON", L"正在播放", BS_GROUPBOX, 9001, app->uiFont);
    app->coverStatic = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
                                       WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE | SS_NOPREFIX,
                                       0, 0, 10, 10, hwnd, nullptr, nullptr, nullptr);
    SetFont(app->coverStatic, app->uiFont);
    app->titleLabel = CreateControl(hwnd, L"STATIC", L"未选择歌曲",
                                    SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, IDC_TITLE,
                                    app->titleFont);
    app->artistLabel = CreateControl(hwnd, L"STATIC", L"添加音乐开始播放",
                                     SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, IDC_ARTIST,
                                     app->uiFont);
    app->albumLabel = CreateControl(hwnd, L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX,
                                    IDC_ALBUM, app->uiFont);
    app->seekBar = CreateControl(hwnd, TRACKBAR_CLASSW, L"",
                                 WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_SEEK, app->uiFont);
    SendMessageW(app->seekBar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
    SendMessageW(app->seekBar, TBM_SETPAGESIZE, 0, 100);
    app->timeLabel = CreateControl(hwnd, L"STATIC", L"00:00 / 00:00",
                                   SS_LEFT | SS_NOPREFIX, IDC_TIME, app->uiFont);
    app->prevButton = CreateControl(hwnd, L"BUTTON", L"上一首", WS_TABSTOP | BS_PUSHBUTTON,
                                    IDC_PREV, app->uiFont);
    app->playButton = CreateControl(hwnd, L"BUTTON", L"播放", WS_TABSTOP | BS_DEFPUSHBUTTON,
                                    IDC_PLAY, app->uiFont);
    app->nextButton = CreateControl(hwnd, L"BUTTON", L"下一首", WS_TABSTOP | BS_PUSHBUTTON,
                                    IDC_NEXT, app->uiFont);
    app->volumeLabel = CreateControl(hwnd, L"STATIC", L"音量 70", SS_LEFT | SS_NOPREFIX,
                                     IDC_VOLUME_LABEL, app->uiFont);
    app->volumeBar = CreateControl(hwnd, TRACKBAR_CLASSW, L"",
                                   WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_VOLUME, app->uiFont);
    SendMessageW(app->volumeBar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(app->volumeBar, TBM_SETPOS, TRUE, app->volume);

    CreateControl(hwnd, L"STATIC", L"播放列表", SS_LEFT | SS_NOPREFIX, 9002, app->uiFont);
    CreateControl(hwnd, L"STATIC", L"歌词", SS_LEFT | SS_NOPREFIX, 9003, app->uiFont);
    app->lyricsSourceLabel = CreateControl(hwnd, L"STATIC", L"未找到歌词",
                                           SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX,
                                           IDC_LYRICS_SOURCE, app->uiFont);
    CreateControl(hwnd, L"BUTTON", L"制作歌词", WS_TABSTOP | BS_PUSHBUTTON, IDC_MAKE_LYRICS,
                  app->uiFont);

    app->listView = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
            LVS_NOSORTHEADER | LVS_OWNERDATA,
        0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IDC_PLAYLIST), nullptr, nullptr);
    ListView_SetExtendedListViewStyle(
        app->listView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    SetFont(app->listView, app->uiFont);
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<wchar_t*>(L"#");
    column.cx = 44;
    ListView_InsertColumn(app->listView, 0, &column);
    column.pszText = const_cast<wchar_t*>(L"歌曲");
    column.cx = 340;
    ListView_InsertColumn(app->listView, 1, &column);
    column.pszText = const_cast<wchar_t*>(L"艺术家");
    column.cx = 160;
    ListView_InsertColumn(app->listView, 2, &column);
    column.pszText = const_cast<wchar_t*>(L"时长");
    column.cx = 62;
    ListView_InsertColumn(app->listView, 3, &column);

    app->lyricsList = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IDC_LYRICS), nullptr, nullptr);
    SetFont(app->lyricsList, app->uiFont);
    SendMessageW(app->lyricsList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"暂无歌词"));

    app->statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd,
        nullptr, app->instance, nullptr);
    SetFont(app->statusBar, app->uiFont);
    int parts[3] = {600, 800, -1};
    SendMessageW(app->statusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

    LayoutMain(app);
    app->RefreshAll(true);
    app->UpdateStatus();
    return hwnd;
}

void App::LayoutMainWindow() { LayoutMain(this); }

void App::RefreshPlaylist() {
    if (!listView) return;
    updatingControls = true;
    ListView_SetItemCountEx(listView, static_cast<int>(playlist.size()), LVSICF_NOSCROLL);
    ListView_SetItemState(listView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < playlist.size()) {
        ListView_SetItemState(listView, selectedIndex, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(listView, selectedIndex, FALSE);
    }
    InvalidateRect(listView, nullptr, FALSE);
    updatingControls = false;
}

void App::RefreshNowPlaying(bool artwork) {
    if (!mainWnd) return;
    SetWindowTextW(titleLabel, CurrentTitle().c_str());
    SetWindowTextW(artistLabel, CurrentArtist().c_str());
    SetWindowTextW(albumLabel, CurrentAlbum().c_str());
    if (artwork) {
        UpdateMainCover();
        if (floatWnd) UpdateFloatCover();
    }
}

void App::RefreshLyrics(bool rebuild) {
    if (!lyricsList) return;
    if (rebuild) {
        SendMessageW(lyricsList, LB_RESETCONTENT, 0, 0);
        if (!lyrics.HasAny()) {
            SendMessageW(lyricsList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"暂无歌词"));
            SetWindowTextW(lyricsSourceLabel, L"未找到歌词");
        } else {
            for (const auto& line : lyrics.Lines()) {
                const wchar_t* text = line.text.empty() ? L"♪" : line.text.c_str();
                SendMessageW(lyricsList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
            }
            std::wstring source = L"来源：" + lyrics.Source();
            SetWindowTextW(lyricsSourceLabel, source.c_str());
        }
        SendMessageW(lyricsList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    }
    RefreshLyricSelection();
}

void App::RefreshLyricSelection() {
    if (!lyricsList || !lyrics.HasAny() || !lyrics.IsTimed()) return;
    int index = lyrics.IndexAt(position);
    if (index < 0 || static_cast<size_t>(index) >= lyrics.LineCount()) return;
    updatingControls = true;
    SendMessageW(lyricsList, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
    int top = std::max(0, index - 2);
    SendMessageW(lyricsList, LB_SETTOPINDEX, static_cast<WPARAM>(top), 0);
    updatingControls = false;
}

void App::RefreshTransport() {
    if (!mainWnd) {
        if (floatWnd) UpdateFloatingWindow(false);
        return;
    }
    updatingControls = true;
    if (playButton) {
        SetWindowTextW(playButton, audio.IsPlaying() ? L"暂停" : L"播放");
    }
    if (seekBar) {
        if (!userSeeking) {
            int value = duration > 0.0
                            ? static_cast<int>(std::max(0.0, std::min(1.0, position / duration)) * 1000.0)
                            : 0;
            SendMessageW(seekBar, TBM_SETPOS, TRUE, value);
        }
    }
    if (timeLabel) {
        std::wstring text = FormatTime(position) + L" / " + FormatTime(duration);
        SetWindowTextW(timeLabel, text.c_str());
    }
    updatingControls = false;
    if (floatWnd) UpdateFloatingWindow(false);
}

void App::RefreshAll(bool artwork) {
    RefreshPlaylist();
    RefreshNowPlaying(artwork);
    RefreshLyrics(true);
    RefreshTransport();
    UpdateStatus();
    if (floatWnd) UpdateFloatingWindow(artwork);
}

void App::UpdateStatus() {
    if (!statusBar) return;
    std::wstring engine = L"引擎：" + audio.BackendName();
    std::wstring count = NumberText(playlist.size()) + L" 首";
    SendMessageW(statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(statusText.c_str()));
    SendMessageW(statusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(engine.c_str()));
    SendMessageW(statusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(count.c_str()));
}

void App::UpdateWindowTitle() {
    if (!mainWnd) return;
    std::wstring title = CurrentTitle();
    if (title.empty()) title = L"MAKASOUNDER";
    else title += L" - MAKASOUNDER";
    SetWindowTextW(mainWnd, title.c_str());
}

void App::UpdateMainCover() {
    if (!coverStatic) return;
    HBITMAP next = CreateCoverBitmap(180);
    HBITMAP old = reinterpret_cast<HBITMAP>(
        SendMessageW(coverStatic, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(next)));
    if (old) DeleteObject(old);
    coverBitmap = next;
    InvalidateRect(coverStatic, nullptr, TRUE);
}

// ------------------------------- Settings dialog -------------------------------

INT_PTR CALLBACK SettingsDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (app && HandleMediaKeys(app, message, wParam, lParam)) return TRUE;
    switch (message) {
        case WM_INITDIALOG: {
            app = reinterpret_cast<App*>(lParam);
            SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LPARAM>(app));
            CheckDlgButton(dialog, IDC_SET_SNAP,
                           app->settings.Data().snapEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckRadioButton(dialog, IDC_SET_MODE_MAIN, IDC_SET_MODE_FLOAT,
                             app->settings.Data().doubleClickFloat ? IDC_SET_MODE_FLOAT
                                                                   : IDC_SET_MODE_MAIN);
            UpdateAssociationLabel(dialog, app);
            RECT ownerRect{};
            RECT dialogRect{};
            GetWindowRect(GetParent(dialog), &ownerRect);
            GetWindowRect(dialog, &dialogRect);
            int width = dialogRect.right - dialogRect.left;
            int height = dialogRect.bottom - dialogRect.top;
            int x = ownerRect.left + (ownerRect.right - ownerRect.left - width) / 2;
            int y = ownerRect.top + (ownerRect.bottom - ownerRect.top - height) / 2;
            SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_SET_REGISTER) {
                bool floatMode = IsDlgButtonChecked(dialog, IDC_SET_MODE_FLOAT) == BST_CHECKED;
                app->ApplyAssociation(true, floatMode, dialog);
                UpdateAssociationLabel(dialog, app);
                return TRUE;
            }
            if (id == IDC_SET_REMOVE) {
                app->ApplyAssociation(false, false, dialog);
                UpdateAssociationLabel(dialog, app);
                return TRUE;
            }
            if (id == IDOK) {
                bool oldMode = app->settings.Data().doubleClickFloat;
                bool newMode = IsDlgButtonChecked(dialog, IDC_SET_MODE_FLOAT) == BST_CHECKED;
                app->settings.Data().snapEnabled =
                    IsDlgButtonChecked(dialog, IDC_SET_SNAP) == BST_CHECKED;
                app->settings.Data().doubleClickFloat = newMode;
                if (oldMode != newMode && QueryAssociation().registered) {
                    std::wstring error;
                    RegisterAssociation(newMode, error);
                }
                app->settings.Save();
                if (!app->settings.Data().snapEnabled) {
                    app->floatDocked = false;
                    if (app->floatCompact) app->SetFloatCompact(false, true);
                }
                EndDialog(dialog, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            break;
        }
        default: break;
    }
    return FALSE;
}

// ------------------------------ Lyric maker dialog -----------------------------

int MakerAssignedCount(const App* app) {
    int assigned = 0;
    for (const auto& line : app->makerLines)
        if (line.ms >= 0) ++assigned;
    return assigned;
}

void FillMakerList(HWND dialog, App* app) {
    HWND list = GetDlgItem(dialog, IDC_MAKER_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < app->makerLines.size(); ++i) {
        const auto& line = app->makerLines[i];
        std::wstring stamp = line.ms >= 0 ? LrcParser::FormatTimestamp(line.ms) : L"--:--.--";
        std::wstring text = stamp + L"  " + (line.text.empty() ? L"（空行）" : line.text);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    if (app->makerSelected >= 0 && static_cast<size_t>(app->makerSelected) < app->makerLines.size()) {
        SendMessageW(list, LB_SETCURSEL, app->makerSelected, 0);
        SendMessageW(list, LB_SETTOPINDEX,
                     static_cast<WPARAM>(std::max(0, app->makerSelected - 4)), 0);
    }
    std::wstring info = L"已录入 " + NumberText(app->makerLines.size()) + L" 行 · 已打轴 " +
                        NumberText(MakerAssignedCount(app)) + L" 行";
    SetDlgItemTextW(dialog, IDC_MAKER_INFO, info.c_str());
}

void UpdateMakerTime(HWND dialog, App* app) {
    if (!app->makerSeeking) {
        SetDlgItemTextW(dialog, IDC_MAKER_TIME, FormatTime(app->position, true).c_str());
        if (app->duration > 0.0) {
            int position = static_cast<int>(
                std::max(0.0, std::min(1.0, app->position / app->duration)) * 1000.0);
            SendDlgItemMessageW(dialog, IDC_MAKER_SEEK, TBM_SETPOS, TRUE, position);
        }
    }
    SetDlgItemTextW(dialog, IDC_MAKER_PLAY, app->audio.IsPlaying() ? L"暂停" : L"播放");
}

bool AddMakerLines(HWND dialog, App* app) {
    HWND edit = GetDlgItem(dialog, IDC_MAKER_INPUT);
    int length = GetWindowTextLengthW(edit);
    if (length <= 0) {
        MessageBeep(MB_ICONASTERISK);
        SetFocus(edit);
        return false;
    }
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    int added = 0;
    for (auto& part : SplitLines(value)) {
        part = Trim(part);
        if (part.empty()) continue;
        app->makerLines.push_back({-1, part});
        ++added;
    }
    if (added == 0) {
        MessageBeep(MB_ICONASTERISK);
        return false;
    }
    app->makerSelected = static_cast<int>(app->makerLines.size()) - 1;
    SetWindowTextW(edit, L"");
    FillMakerList(dialog, app);
    SetFocus(edit);
    return true;
}

bool DeleteMakerLine(HWND dialog, App* app) {
    if (app->makerSelected < 0 ||
        static_cast<size_t>(app->makerSelected) >= app->makerLines.size()) {
        MessageBeep(MB_ICONASTERISK);
        return false;
    }
    app->makerLines.erase(app->makerLines.begin() + app->makerSelected);
    app->makerUndo.clear();
    if (app->makerLines.empty()) app->makerSelected = -1;
    else if (app->makerSelected >= static_cast<int>(app->makerLines.size()))
        app->makerSelected = static_cast<int>(app->makerLines.size()) - 1;
    FillMakerList(dialog, app);
    SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
    return true;
}

bool TapMakerLine(HWND dialog, App* app) {
    if (!app->audio.IsLoaded()) {
        MessageBoxW(dialog, L"请先播放歌曲，再按当前时间打轴。", L"打轴",
                    MB_ICONINFORMATION | MB_OK);
        return false;
    }
    if (app->makerLines.empty()) {
        MessageBoxW(dialog, L"请先在右侧输入歌词行。", L"打轴", MB_ICONINFORMATION | MB_OK);
        return false;
    }
    if (app->makerSelected < 0 ||
        static_cast<size_t>(app->makerSelected) >= app->makerLines.size()) {
        app->makerSelected = 0;
    }
    app->makerUndo.push_back(
        {app->makerSelected, app->makerLines[app->makerSelected].ms});
    app->makerLines[app->makerSelected].ms =
        static_cast<int>(app->position * 1000.0 + 0.5);
    if (app->makerSelected + 1 < static_cast<int>(app->makerLines.size()))
        ++app->makerSelected;
    FillMakerList(dialog, app);
    SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
    return true;
}

bool StartMakerSecondPass(HWND dialog, App* app) {
    if (!app->audio.IsLoaded()) {
        MessageBoxW(dialog, L"请先播放歌曲，再从头开始打轴。", L"第二遍",
                    MB_ICONINFORMATION | MB_OK);
        return false;
    }
    if (app->makerLines.empty()) {
        MessageBoxW(dialog, L"请先在右侧输入歌词行。", L"第二遍", MB_ICONINFORMATION | MB_OK);
        return false;
    }
    app->SeekTo(0.0);
    if (!app->audio.IsPlaying()) app->TogglePlayback();
    int first = 0;
    for (size_t i = 0; i < app->makerLines.size(); ++i) {
        if (app->makerLines[i].ms < 0) {
            first = static_cast<int>(i);
            break;
        }
    }
    app->makerSelected = first;
    FillMakerList(dialog, app);
    SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
    return true;
}

void SeekMakerLine(HWND dialog, App* app) {
    if (app->makerSelected < 0 ||
        static_cast<size_t>(app->makerSelected) >= app->makerLines.size())
        return;
    const auto& line = app->makerLines[app->makerSelected];
    if (line.ms < 0) return;
    app->SeekTo(line.ms / 1000.0);
    if (!app->audio.IsPlaying()) app->TogglePlayback();
    SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
}

LRESULT CALLBACK MakerChildProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    int id = GetDlgCtrlID(hwnd);
    if (message == WM_KEYDOWN) {
        if (wParam == VK_F8) {
            HWND dialog = GetParent(hwnd);
            App* app = reinterpret_cast<App*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
            if (app) TapMakerLine(dialog, app);
            return 0;
        }
        if (wParam == VK_RETURN) {
            PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_MAKER_TAP, BN_CLICKED),
                         reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
        if (wParam == VK_DELETE && id == IDC_MAKER_LIST) {
            PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_MAKER_DELETE, BN_CLICKED),
                         reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
    }
    WNDPROC original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return original ? CallWindowProcW(original, hwnd, message, wParam, lParam)
                    : DefWindowProcW(hwnd, message, wParam, lParam);
}

void SubclassMakerChild(HWND dialog, int id) {
    HWND child = GetDlgItem(dialog, id);
    if (!child) return;
    WNDPROC original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(child, GWLP_WNDPROC));
    SetWindowLongPtrW(child, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(original));
    SetWindowLongPtrW(child, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MakerChildProc));
}

bool App::SaveMakerLrc(HWND owner) {
    Track* track = CurrentTrack();
    if (!track) return false;
    int assigned = 0;
    for (const auto& line : makerLines)
        if (line.ms >= 0) ++assigned;
    if (assigned == 0) {
        MessageBoxW(owner, L"还没有打轴的歌词行。请先完成第二遍打轴。", L"保存 LRC",
                    MB_ICONINFORMATION | MB_OK);
        return false;
    }
    int unassigned = static_cast<int>(makerLines.size()) - assigned;
    if (unassigned > 0) {
        std::wstring message = L"还有 " + NumberText(unassigned) +
                               L" 行没有时间轴，保存时会忽略这些行。继续吗？";
        if (MessageBoxW(owner, message.c_str(), L"保存 LRC", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return false;
    }
    std::wstring path = JoinPath(DirNameOf(track->path), StemOf(track->path) + L".lrc");
    if (FileExists(path) &&
        MessageBoxW(owner, L"同名 LRC 已存在，要覆盖吗？", L"保存 LRC",
                    MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return false;
    }
    std::wstringstream stream;
    stream << L"[ti:" << CurrentTitle() << L"]\n";
    stream << L"[ar:" << CurrentArtist() << L"]\n";
    if (!CurrentAlbum().empty()) stream << L"[al:" << CurrentAlbum() << L"]\n";
    stream << L"[by:MAKASOUNDER]\n\n";
    for (const auto& line : makerLines) {
        if (line.ms < 0) continue;
        stream << LrcParser::FormatTimestamp(line.ms) << line.text << L"\n";
    }
    std::wstring text = stream.str();
    if (!WriteUtf8File(path, text, true)) {
        MessageBoxW(owner, L"写入歌词文件失败。", L"保存 LRC", MB_ICONERROR | MB_OK);
        return false;
    }
    lyrics.MutableLines().clear();
    for (const auto& line : makerLines) {
        if (line.ms >= 0) lyrics.MutableLines().push_back({line.ms, line.text});
    }
    lyrics.SetTimed(true);
    lyrics.SetSource(L"已保存 LRC");
    lyrics.SetRawText(text);
    lastLyricIndex = -2;
    RefreshLyrics(true);
    statusText = L"歌词已保存：" + FileNameOf(path);
    UpdateStatus();
    return true;
}

INT_PTR CALLBACK MakerDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (app && HandleMediaKeys(app, message, wParam, lParam)) return TRUE;
    switch (message) {
        case WM_INITDIALOG: {
            app = reinterpret_cast<App*>(lParam);
            SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LPARAM>(app));
            app->makerUndo.clear();
            app->makerSeeking = false;
            SendDlgItemMessageW(dialog, IDC_MAKER_SEEK, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
            FillMakerList(dialog, app);
            UpdateMakerTime(dialog, app);
            SubclassMakerChild(dialog, IDC_MAKER_INPUT);
            SubclassMakerChild(dialog, IDC_MAKER_LIST);
            SetTimer(dialog, 1, 500, nullptr);
            SetFocus(GetDlgItem(dialog, IDC_MAKER_INPUT));
            return TRUE;
        }
        case WM_TIMER:
            UpdateMakerTime(dialog, app);
            return TRUE;
        case WM_HSCROLL: {
            if (reinterpret_cast<HWND>(lParam) == GetDlgItem(dialog, IDC_MAKER_SEEK)) {
                if (app->duration <= 0.0) return TRUE;
                int value = static_cast<int>(
                    SendDlgItemMessageW(dialog, IDC_MAKER_SEEK, TBM_GETPOS, 0, 0));
                double target = app->duration * value / 1000.0;
                int code = LOWORD(wParam);
                if (code == TB_THUMBTRACK) {
                    app->makerSeeking = true;
                    SetDlgItemTextW(dialog, IDC_MAKER_TIME,
                                    FormatTime(target, true).c_str());
                } else {
                    app->makerSeeking = false;
                    app->SeekTo(target);
                    UpdateMakerTime(dialog, app);
                }
                return TRUE;
            }
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_F8) {
                TapMakerLine(dialog, app);
                return TRUE;
            }
            break;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_MAKER_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
                int selection = static_cast<int>(
                    SendDlgItemMessageW(dialog, IDC_MAKER_LIST, LB_GETCURSEL, 0, 0));
                if (selection >= 0) app->makerSelected = selection;
                return TRUE;
            }
            if (id == IDC_MAKER_LIST && HIWORD(wParam) == LBN_DBLCLK) {
                int selection = static_cast<int>(
                    SendDlgItemMessageW(dialog, IDC_MAKER_LIST, LB_GETCURSEL, 0, 0));
                if (selection >= 0) app->makerSelected = selection;
                SeekMakerLine(dialog, app);
                return TRUE;
            }
            if (id == IDC_MAKER_TAP) {
                // The "打轴" button is the default button.  If the caret is in
                // the lyric input, Enter means "add line" instead.
                if (GetFocus() == GetDlgItem(dialog, IDC_MAKER_INPUT)) {
                    AddMakerLines(dialog, app);
                } else {
                    TapMakerLine(dialog, app);
                }
                return TRUE;
            }
            if (id == IDC_MAKER_ADD) {
                AddMakerLines(dialog, app);
                return TRUE;
            }
            if (id == IDC_MAKER_DELETE) {
                DeleteMakerLine(dialog, app);
                return TRUE;
            }
            if (id == IDC_MAKER_UNDO) {
                if (!app->makerUndo.empty()) {
                    auto action = app->makerUndo.back();
                    app->makerUndo.pop_back();
                    if (action.first >= 0 &&
                        static_cast<size_t>(action.first) < app->makerLines.size()) {
                        app->makerLines[action.first].ms = action.second;
                        app->makerSelected = action.first;
                        FillMakerList(dialog, app);
                        SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
                    }
                }
                return TRUE;
            }
            if (id == IDC_MAKER_RESTART) {
                StartMakerSecondPass(dialog, app);
                return TRUE;
            }
            if (id == IDC_MAKER_IMPORT) {
                wchar_t file[MAX_PATH] = {};
                OPENFILENAMEW open{};
                open.lStructSize = sizeof(open);
                open.hwndOwner = dialog;
                open.lpstrFilter = L"LRC 歌词 (*.lrc)\0*.lrc\0文本文件 (*.txt)\0*.txt\0所有文件\0*.*\0\0";
                open.lpstrFile = file;
                open.nMaxFile = MAX_PATH;
                open.lpstrTitle = L"导入歌词";
                open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
                if (!GetOpenFileNameW(&open)) return TRUE;
                std::vector<uint8_t> bytes;
                if (!ReadWholeFile(file, bytes, 8u * 1024u * 1024u)) {
                    MessageBoxW(dialog, L"无法读取该文件。", L"导入歌词", MB_ICONERROR | MB_OK);
                    return TRUE;
                }
                LrcParseResult parsed = LrcParser::Parse(DecodeTextFileBytes(bytes));
                app->makerLines.clear();
                app->makerUndo.clear();
                for (const auto& line : parsed.lines)
                    app->makerLines.push_back({line.timeMs, line.text});
                app->makerSelected = app->makerLines.empty() ? -1 : 0;
                FillMakerList(dialog, app);
                SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
                return TRUE;
            }
            if (id == IDC_MAKER_SAVE) {
                if (app->SaveMakerLrc(dialog)) FillMakerList(dialog, app);
                return TRUE;
            }
            if (id == IDC_MAKER_PLAY) {
                app->TogglePlayback();
                UpdateMakerTime(dialog, app);
                SetFocus(GetDlgItem(dialog, IDC_MAKER_LIST));
                return TRUE;
            }
            if (id == IDC_MAKER_CLOSE || id == IDCANCEL) {
                app->makerSeeking = false;
                KillTimer(dialog, 1);
                EndDialog(dialog, IDOK);
                return TRUE;
            }
            break;
        }
        default: break;
    }
    return FALSE;
}

void MainWindowRefresh(App* app) {
    if (app->mainWnd) InvalidateRect(app->mainWnd, nullptr, FALSE);
}

}  // namespace mk
