#include "app.h"

#include "common.h"
#include "resource.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace mk {

namespace {

constexpr int kFloatExpandedWidth = 380;
constexpr int kFloatExpandedHeight = 126;
constexpr int kFloatCompactSize = 72;
constexpr UINT_PTR kFloatAnimTimer = 2;
constexpr UINT_PTR kFloatHoverTimer = 3;
constexpr UINT_PTR kFloatDockWatchTimer = 4;
constexpr DWORD kFloatAnimDurationMs = 240;
constexpr int kFloatCoverPixels = 256;

int FloatWidth(App* app) { return app->floatCompact ? kFloatCompactSize : kFloatExpandedWidth; }
int FloatHeight(App* app) { return app->floatCompact ? kFloatCompactSize : kFloatExpandedHeight; }

void SetTextIfChanged(HWND control, const std::wstring& text) {
    if (!control) return;
    wchar_t current[512] = {};
    GetWindowTextW(control, current, 512);
    if (text != current) SetWindowTextW(control, text.c_str());
}

void CreateFloatControls(App* app) {
    HWND parent = app->floatWnd;
    app->floatCover = CreateWindowExW(
        WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOPREFIX, 0,
        0, 10, 10, parent, reinterpret_cast<HMENU>(IDC_FLOAT_COVER), nullptr, nullptr);
    app->floatTitle = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 0, 0, 10,
        10, parent, nullptr, nullptr, nullptr);
    app->floatArtist = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 0, 0, 10,
        10, parent, nullptr, nullptr, nullptr);
    app->floatAlbum = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 0, 0, 10,
        10, parent, nullptr, nullptr, nullptr);
    app->floatLyric = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 0, 0, 10,
        10, parent, nullptr, nullptr, nullptr);
    app->floatPrev = CreateWindowExW(
        0, L"BUTTON", L"上一首", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10,
        parent, reinterpret_cast<HMENU>(IDC_PREV), nullptr, nullptr);
    app->floatPlay = CreateWindowExW(
        0, L"BUTTON", L"播放", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 10, 10,
        parent, reinterpret_cast<HMENU>(IDC_PLAY), nullptr, nullptr);
    app->floatNext = CreateWindowExW(
        0, L"BUTTON", L"下一首", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10,
        parent, reinterpret_cast<HMENU>(IDC_NEXT), nullptr, nullptr);
    SendMessageW(app->floatTitle, WM_SETFONT, reinterpret_cast<WPARAM>(app->titleFont), TRUE);
    SendMessageW(app->floatArtist, WM_SETFONT, reinterpret_cast<WPARAM>(app->uiFont), TRUE);
    SendMessageW(app->floatAlbum, WM_SETFONT, reinterpret_cast<WPARAM>(app->uiFont), TRUE);
    SendMessageW(app->floatLyric, WM_SETFONT, reinterpret_cast<WPARAM>(app->uiFont), TRUE);
    SendMessageW(app->floatPrev, WM_SETFONT, reinterpret_cast<WPARAM>(app->uiFont), TRUE);
    SendMessageW(app->floatPlay, WM_SETFONT, reinterpret_cast<WPARAM>(app->uiFont), TRUE);
    SendMessageW(app->floatNext, WM_SETFONT, reinterpret_cast<WPARAM>(app->uiFont), TRUE);
}

void ApplyFloatLayout(App* app) {
    if (!app->floatWnd) return;
    RECT client{};
    GetClientRect(app->floatWnd, &client);
    int width = client.right;
    int height = client.bottom;
    if (app->floatCompact || app->floatAnimating) {
        ShowWindow(app->floatTitle, SW_HIDE);
        ShowWindow(app->floatArtist, SW_HIDE);
        ShowWindow(app->floatAlbum, SW_HIDE);
        ShowWindow(app->floatLyric, SW_HIDE);
        ShowWindow(app->floatPrev, SW_HIDE);
        ShowWindow(app->floatPlay, SW_HIDE);
        ShowWindow(app->floatNext, SW_HIDE);
        int inset = 3;
        MoveWindow(app->floatCover, inset, inset, width - inset * 2, height - inset * 2, TRUE);
        ShowWindow(app->floatCover, SW_SHOW);
        return;
    }
    ShowWindow(app->floatCover, SW_SHOW);
    ShowWindow(app->floatTitle, SW_SHOW);
    ShowWindow(app->floatArtist, SW_SHOW);
    ShowWindow(app->floatAlbum, SW_SHOW);
    ShowWindow(app->floatLyric, SW_SHOW);
    ShowWindow(app->floatPrev, SW_SHOW);
    ShowWindow(app->floatPlay, SW_SHOW);
    ShowWindow(app->floatNext, SW_SHOW);
    int cover = 80;
    MoveWindow(app->floatCover, 8, 8, cover, cover, TRUE);
    int textX = 96;
    int textW = std::max(120, width - textX - 8);
    MoveWindow(app->floatTitle, textX, 8, textW, 22, TRUE);
    MoveWindow(app->floatArtist, textX, 31, textW, 17, TRUE);
    MoveWindow(app->floatAlbum, textX, 49, textW, 17, TRUE);
    MoveWindow(app->floatLyric, textX, 69, textW, 17, TRUE);
    int buttonY = 92;
    MoveWindow(app->floatPrev, textX, buttonY, 62, 24, TRUE);
    MoveWindow(app->floatPlay, textX + 68, buttonY, 72, 24, TRUE);
    MoveWindow(app->floatNext, textX + 146, buttonY, 62, 24, TRUE);
}

RECT CoverScreenRect(const RECT& window, bool compact) {
    if (compact) {
        return RECT{window.left + 3, window.top + 3, window.right - 3, window.bottom - 3};
    }
    return RECT{window.left + 8, window.top + 8, window.left + 88, window.top + 88};
}

RECT LerpRect(const RECT& a, const RECT& b, double t) {
    return RECT{
        static_cast<LONG>(a.left + (b.left - a.left) * t),
        static_cast<LONG>(a.top + (b.top - a.top) * t),
        static_cast<LONG>(a.right + (b.right - a.right) * t),
        static_cast<LONG>(a.bottom + (b.bottom - a.bottom) * t),
    };
}

void ApplyFloatRegion(HWND hwnd, int width, int height, double radiusFactor) {
    if (radiusFactor <= 0.01) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }
    int rx = std::max(1, static_cast<int>(width * radiusFactor));
    int ry = std::max(1, static_cast<int>(height * radiusFactor));
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, rx, ry);
    if (region) SetWindowRgn(hwnd, region, FALSE);
}

void HideFloatTextControls(App* app) {
    ShowWindow(app->floatTitle, SW_HIDE);
    ShowWindow(app->floatArtist, SW_HIDE);
    ShowWindow(app->floatAlbum, SW_HIDE);
    ShowWindow(app->floatLyric, SW_HIDE);
    ShowWindow(app->floatPrev, SW_HIDE);
    ShowWindow(app->floatPlay, SW_HIDE);
    ShowWindow(app->floatNext, SW_HIDE);
    ShowWindow(app->floatCover, SW_SHOW);
}

void LayoutFloatAnimationFrame(App* app, double eased) {
    RECT window = LerpRect(app->floatAnimStartRect, app->floatAnimEndRect, eased);
    int width = window.right - window.left;
    int height = window.bottom - window.top;
    SetWindowPos(app->floatWnd, HWND_TOPMOST, window.left, window.top, width, height,
                 SWP_NOACTIVATE);
    RECT startCover = CoverScreenRect(app->floatAnimStartRect, app->floatAnimStartCompact);
    RECT endCover = CoverScreenRect(app->floatAnimEndRect, app->floatAnimTargetCompact);
    RECT cover = LerpRect(startCover, endCover, eased);
    MoveWindow(app->floatCover, cover.left - window.left, cover.top - window.top,
               cover.right - cover.left, cover.bottom - cover.top, TRUE);
    double radius = 0.0;
    if (app->floatAnimStartCompact && app->floatAnimTargetCompact) radius = 1.0;
    else if (app->floatAnimTargetCompact) radius = eased;
    else if (app->floatAnimStartCompact) radius = 1.0 - eased;
    ApplyFloatRegion(app->floatWnd, width, height, radius);
    InvalidateRect(app->floatCover, nullptr, TRUE);
}

void StopFloatHoverTimers(App* app) {
    if (!app->floatWnd) return;
    KillTimer(app->floatWnd, kFloatHoverTimer);
    KillTimer(app->floatWnd, kFloatDockWatchTimer);
    app->floatHoverPending = false;
    app->floatDockWatchActive = false;
    app->floatOutsideChecks = 0;
}

void StartFloatDockWatch(App* app) {
    if (!app->floatWnd || !app->floatDocked || app->floatCompact || app->floatAnimating ||
        !IsWindowVisible(app->floatWnd))
        return;
    if (!app->floatDockWatchActive) {
        if (SetTimer(app->floatWnd, kFloatDockWatchTimer, 250, nullptr)) {
            app->floatDockWatchActive = true;
            app->floatOutsideChecks = 0;
        }
    }
}

void ScheduleFloatHover(App* app) {
    if (!app->floatWnd || !app->floatDocked || !app->floatCompact || app->floatAnimating ||
        app->floatHoverPending || app->floatBallPressed)
        return;
    app->floatHoverPending = SetTimer(app->floatWnd, kFloatHoverTimer, 220, nullptr) != 0;
    if (app->floatHoverPending) {
        TRACKMOUSEEVENT mouse{};
        mouse.cbSize = sizeof(mouse);
        mouse.dwFlags = TME_LEAVE;
        mouse.hwndTrack = app->floatWnd;
        TrackMouseEvent(&mouse);
    }
}

void FinishFloatAnimation(App* app) {
    if (!app->floatWnd || !app->floatAnimating) return;
    KillTimer(app->floatWnd, kFloatAnimTimer);
    RECT window = app->floatAnimEndRect;
    int width = window.right - window.left;
    int height = window.bottom - window.top;
    SetWindowPos(app->floatWnd, HWND_TOPMOST, window.left, window.top, width, height,
                 SWP_NOACTIVATE);
    app->floatAnimating = false;
    app->floatCompact = app->floatAnimTargetCompact;
    if (app->floatCompact) {
        ApplyFloatRegion(app->floatWnd, width, height, 1.0);
        app->settings.Data().floatCompact = app->floatDocked;
    } else {
        SetWindowRgn(app->floatWnd, nullptr, TRUE);
        app->settings.Data().floatCompact = app->floatDocked;
    }
    ApplyFloatLayout(app);
    app->UpdateFloatingWindow(false);
    InvalidateRect(app->floatWnd, nullptr, TRUE);
    StopFloatHoverTimers(app);
    if (app->floatDocked && !app->floatCompact) StartFloatDockWatch(app);
    if (app->floatAnimPersist) app->SaveFloatPosition();
}

void TickFloatAnimation(App* app) {
    if (!app->floatWnd || !app->floatAnimating) return;
    DWORD elapsed = GetTickCount() - app->floatAnimStartTick;
    double progress = std::min(1.0, static_cast<double>(elapsed) / kFloatAnimDurationMs);
    double eased = 1.0 - std::pow(1.0 - progress, 3.0);
    LayoutFloatAnimationFrame(app, eased);
    if (progress >= 1.0) FinishFloatAnimation(app);
}

void BeginFloatAnimation(App* app, bool compact, bool persist, const RECT& endRect) {
    if (!app->floatWnd || app->floatAnimating) return;
    RECT startRect{};
    GetWindowRect(app->floatWnd, &startRect);
    if (app->floatCompact == compact && EqualRect(&startRect, &endRect)) {
        if (persist) app->SaveFloatPosition();
        return;
    }
    app->floatAnimating = true;
    app->floatAnimTargetCompact = compact;
    app->floatAnimPersist = persist;
    app->floatAnimStartCompact = app->floatCompact;
    app->floatAnimStartTick = GetTickCount();
    app->floatAnimStartRect = startRect;
    app->floatAnimEndRect = endRect;
    StopFloatHoverTimers(app);
    HideFloatTextControls(app);
    LayoutFloatAnimationFrame(app, 0.0);
    SetTimer(app->floatWnd, kFloatAnimTimer, 16, nullptr);
}


void UpdateFloatText(App* app) {
    if (!app->floatWnd) return;
    SetTextIfChanged(app->floatTitle, app->CurrentTitle());
    SetTextIfChanged(app->floatArtist, app->CurrentArtist());
    SetTextIfChanged(app->floatAlbum, app->CurrentAlbum());
    std::wstring lyric;
    int index = app->lyrics.IndexAt(app->position);
    if (index >= 0) {
        const LyricLine* line = app->lyrics.LineAt(index);
        if (line && !line->text.empty()) lyric = line->text;
    }
    if (lyric.empty()) lyric = app->lyrics.HasAny() ? L"♪" : L"暂无歌词";
    SetTextIfChanged(app->floatLyric, lyric);
    SetTextIfChanged(app->floatPlay, app->audio.IsPlaying() ? L"暂停" : L"播放");
    std::wstring title = app->CurrentTitle() + L" - MAKASOUNDER 悬浮窗";
    SetTextIfChanged(app->floatWnd, title);
}

bool NearEdge(App* app, int& edge, RECT& work) {
    RECT rect{};
    GetWindowRect(app->floatWnd, &rect);
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return false;
    work = info.rcWork;
    int threshold = 16;
    int distances[4] = {
        std::abs(static_cast<int>(rect.left - work.left)),
        std::abs(static_cast<int>(work.right - rect.right)),
        std::abs(static_cast<int>(rect.top - work.top)),
        std::abs(static_cast<int>(work.bottom - rect.bottom)),
    };
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (distances[i] < distances[best]) best = i;
    if (distances[best] > threshold) return false;
    edge = 1 << best;
    return true;
}

void ClampFloat(App* app) {
    if (!app->floatWnd) return;
    RECT rect{};
    GetWindowRect(app->floatWnd, &rect);
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = std::max(static_cast<int>(info.rcWork.left),
                     std::min(static_cast<int>(rect.left),
                              static_cast<int>(info.rcWork.right) - width));
    int y = std::max(static_cast<int>(info.rcWork.top),
                     std::min(static_cast<int>(rect.top),
                              static_cast<int>(info.rcWork.bottom) - height));
    if (x != rect.left || y != rect.top) {
        SetWindowPos(app->floatWnd, HWND_TOPMOST, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void HandleFloatDrop(App* app) {
    if (!app->floatWnd) return;
    RECT work{};
    int edge = 0;
    bool snapped = app->settings.Data().snapEnabled && NearEdge(app, edge, work);
    if (snapped) {
        app->floatSnapEdge = edge;
        app->floatDocked = true;
        app->SetFloatCompact(true, true);
        return;
    }
    app->floatSnapEdge = 0;
    app->floatDocked = false;
    if (app->floatCompact) {
        app->SetFloatCompact(false, true);
        return;
    }
    ClampFloat(app);
    app->settings.Data().floatCompact = false;
    app->SaveFloatPosition();
}

void ShowFloatMenu(App* app, POINT screen) {
    KillTimer(app->floatWnd, kFloatHoverTimer);
    app->floatHoverPending = false;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, 1, L"返回主界面");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 2, app->floatCompact ? L"展开悬浮窗" : L"收为小窗");
    AppendMenuW(menu, MF_STRING | (app->settings.Data().snapEnabled ? MF_CHECKED : MF_UNCHECKED),
                3, L"拖动到边缘自动缩略");
    AppendMenuW(menu, MF_STRING, 4, L"重置位置");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 5, L"退出 MAKASOUNDER");
    SetForegroundWindow(app->floatWnd);
    app->floatMenuOpen = true;
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0,
                                 app->floatWnd, nullptr);
    app->floatMenuOpen = false;
    DestroyMenu(menu);
    switch (command) {
        case 1: app->ReturnToMainFromFloat(); break;
        case 2:
            if (app->floatCompact) {
                app->floatDocked = true;
                app->SetFloatCompact(false, false);
            } else {
                app->floatDocked = true;
                app->SetFloatCompact(true, true);
            }
            break;
        case 3:
            app->settings.Data().snapEnabled = !app->settings.Data().snapEnabled;
            app->settings.Save();
            if (!app->settings.Data().snapEnabled && app->floatCompact) {
                app->floatDocked = false;
                app->SetFloatCompact(false, true);
            }
            break;
        case 4: app->ResetFloatingPosition(); break;
        case 5: app->RequestQuit(); break;
        default: break;
    }
    PostMessageW(app->floatWnd, WM_NULL, 0, 0);
}

LRESULT CALLBACK FloatWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }
    if (!app) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
        case WM_NCHITTEST: {
            if (app->floatAnimating) return HTCLIENT;
            if (app->floatCompact) return HTCLIENT;
            POINT screen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            POINT client = screen;
            ScreenToClient(hwnd, &client);
            HWND child = ChildWindowFromPointEx(hwnd, client, CWP_SKIPINVISIBLE);
            if (child && (child == app->floatPrev || child == app->floatPlay ||
                          child == app->floatNext)) {
                return HTCLIENT;
            }
            return HTCAPTION;
        }
        case WM_NCLBUTTONDBLCLK:
            if (app->floatAnimating) return 0;
            if (wParam == HTCAPTION && app->floatCompact) {
                app->SetFloatCompact(false, true);
                return 0;
            }
            break;
        case WM_EXITSIZEMOVE:
            HandleFloatDrop(app);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && app->floatCompact) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
            if (app->floatCompact && !app->floatAnimating) {
                KillTimer(hwnd, kFloatHoverTimer);
                app->floatHoverPending = false;
                POINT screen{};
                GetCursorPos(&screen);
                GetWindowRect(hwnd, &app->floatBallStartRect);
                app->floatBallStartPoint = screen;
                app->floatBallOffsetX = screen.x - app->floatBallStartRect.left;
                app->floatBallOffsetY = screen.y - app->floatBallStartRect.top;
                app->floatBallPressed = true;
                app->floatBallMoved = false;
                SetCapture(hwnd);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (app->floatBallPressed && GetCapture() == hwnd) {
                POINT screen{};
                GetCursorPos(&screen);
                int dx = screen.x - app->floatBallStartPoint.x;
                int dy = screen.y - app->floatBallStartPoint.y;
                if (!app->floatBallMoved && (std::abs(dx) > 4 || std::abs(dy) > 4)) {
                    app->floatBallMoved = true;
                }
                if (app->floatBallMoved) {
                    SetWindowPos(hwnd, HWND_TOPMOST, screen.x - app->floatBallOffsetX,
                                 screen.y - app->floatBallOffsetY, 0, 0,
                                 SWP_NOSIZE | SWP_NOACTIVATE);
                }
                return 0;
            }
            if (app->floatCompact) ScheduleFloatHover(app);
            break;
        case WM_MOUSELEAVE:
            KillTimer(hwnd, kFloatHoverTimer);
            app->floatHoverPending = false;
            break;
        case WM_LBUTTONUP:
            if (app->floatBallPressed) {
                app->floatBallPressed = false;
                bool moved = app->floatBallMoved;
                app->floatBallMoved = false;
                if (GetCapture() == hwnd) ReleaseCapture();
                if (moved) HandleFloatDrop(app);
                else app->SetFloatCompact(false, false);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            app->floatBallPressed = false;
            app->floatBallMoved = false;
            break;
        case WM_DRAWITEM: {
            if (wParam == IDC_FLOAT_COVER) {
                auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(COLOR_3DFACE));
                bool round = app->floatCompact && !app->floatAnimating;
                HRGN clip = nullptr;
                if (round) {
                    clip = CreateEllipticRgn(draw->rcItem.left, draw->rcItem.top,
                                             draw->rcItem.right + 1, draw->rcItem.bottom + 1);
                    if (clip) SelectClipRgn(draw->hDC, clip);
                }
                if (app->floatCoverBitmap) {
                    HDC memory = CreateCompatibleDC(draw->hDC);
                    HGDIOBJ old = SelectObject(memory, app->floatCoverBitmap);
                    SetStretchBltMode(draw->hDC, HALFTONE);
                    SetBrushOrgEx(draw->hDC, 0, 0, nullptr);
                    StretchBlt(draw->hDC, draw->rcItem.left, draw->rcItem.top,
                               draw->rcItem.right - draw->rcItem.left,
                               draw->rcItem.bottom - draw->rcItem.top, memory, 0, 0,
                               kFloatCoverPixels, kFloatCoverPixels, SRCCOPY);
                    SelectObject(memory, old);
                    DeleteDC(memory);
                }
                if (clip) {
                    SelectClipRgn(draw->hDC, nullptr);
                    DeleteObject(clip);
                }
                return TRUE;
            }
            break;
        }
        case WM_TIMER:
            if (wParam == kFloatAnimTimer) {
                TickFloatAnimation(app);
                return 0;
            }
            if (wParam == kFloatHoverTimer) {
                app->floatHoverPending = false;
                KillTimer(hwnd, kFloatHoverTimer);
                if (app->floatDocked && app->floatCompact && !app->floatAnimating) {
                    POINT cursor{};
                    RECT rect{};
                    GetCursorPos(&cursor);
                    GetWindowRect(hwnd, &rect);
                    if (PtInRect(&rect, cursor)) app->SetFloatCompact(false, false);
                }
                return 0;
            }
            if (wParam == kFloatDockWatchTimer) {
                if (!app->floatDocked || app->floatCompact || app->floatAnimating ||
                    !IsWindowVisible(hwnd)) {
                    KillTimer(hwnd, kFloatDockWatchTimer);
                    app->floatDockWatchActive = false;
                    app->floatOutsideChecks = 0;
                    return 0;
                }
                if (app->floatMenuOpen) return 0;
                POINT cursor{};
                RECT rect{};
                GetCursorPos(&cursor);
                GetWindowRect(hwnd, &rect);
                if (PtInRect(&rect, cursor)) {
                    app->floatOutsideChecks = 0;
                } else if (++app->floatOutsideChecks >= 2) {
                    app->floatOutsideChecks = 0;
                    app->SetFloatCompact(true, false);
                }
                return 0;
            }
            break;
        case WM_ENTERSIZEMOVE:
            StopFloatHoverTimers(app);
            break;
        case WM_APPCOMMAND:
            if (HandleMediaKeys(app, message, wParam, lParam)) return 0;
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (HandleMediaKeys(app, message, wParam, lParam)) return 0;
            break;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_PREV:
                case IDM_PLAY_PREV: app->PlayPrev(); return 0;
                case IDC_PLAY:
                case IDM_PLAY_TOGGLE: app->TogglePlayback(); return 0;
                case IDC_NEXT:
                case IDM_PLAY_NEXT: app->PlayNext(true); return 0;
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
                case IDM_FILE_ADD: app->AddFilesFromDialog(); return 0;
                case IDM_FILE_ADD_FOLDER: app->AddFolderFromDialog(); return 0;
                case IDM_TOOLS_SETTINGS: app->OpenSettings(hwnd); return 0;
                case IDM_FILE_EXIT: app->RequestQuit(); return 0;
                default: break;
            }
            break;
        }
        case WM_RBUTTONUP:
        case WM_NCRBUTTONUP: {
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (message == WM_RBUTTONUP) ClientToScreen(hwnd, &p);
            ShowFloatMenu(app, p);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            app->SetVolume(app->volume + delta / WHEEL_DELTA * 4);
            app->SaveSettings();
            return 0;
        }
        case WM_SHOWWINDOW:
            if (!wParam) {
                StopFloatHoverTimers(app);
            } else if (app->floatDocked && !app->floatCompact) {
                StartFloatDockWatch(app);
            }
            return 0;
        case WM_DISPLAYCHANGE:
            ClampFloat(app);
            return 0;
        case WM_CLOSE:
            app->RequestQuit();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kFloatAnimTimer);
            app->floatWnd = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void RegisterFloatClass(App* app) {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_DBLCLKS;
    windowClass.lpfnWndProc = FloatWndProc;
    windowClass.hInstance = app->instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
    windowClass.lpszClassName = kFloatWindowClass;
    if (RegisterClassExW(&windowClass)) registered = true;
}

}  // namespace

void App::CreateFloatingWindow() {
    if (!floatWnd || !IsWindow(floatWnd)) floatWnd = FloatWindowCreate(this);
}

HWND FloatWindowCreate(App* app) {
    RegisterFloatClass(app);
    app->floatCompact = app->settings.Data().floatCompact;
    app->floatDocked = app->floatCompact;
    int width = FloatWidth(app);
    int height = FloatHeight(app);
    int x = 0;
    int y = 0;
    bool placed = false;
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR cursorMonitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(cursorMonitor, &info)) work = info.rcWork;
    if (app->settings.Data().hasFloatPos) {
        RECT saved{app->settings.Data().floatX, app->settings.Data().floatY,
                   app->settings.Data().floatX + width, app->settings.Data().floatY + height};
        HMONITOR savedMonitor = MonitorFromRect(&saved, MONITOR_DEFAULTTONULL);
        if (savedMonitor && GetMonitorInfoW(savedMonitor, &info)) {
            work = info.rcWork;
            x = saved.left;
            y = saved.top;
            placed = true;
        }
    }
    if (!placed) {
        x = work.right - width - 24;
        y = work.bottom - height - 24;
    }
    x = std::max(static_cast<int>(work.left),
                 std::min(x, static_cast<int>(work.right) - width));
    y = std::max(static_cast<int>(work.top),
                 std::min(y, static_cast<int>(work.bottom) - height));
    int style = WS_POPUP | WS_BORDER;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kFloatWindowClass,
                                L"MAKASOUNDER 悬浮窗", style, x, y, width, height, nullptr, nullptr,
                                app->instance, app);
    if (!hwnd) return nullptr;
    app->floatWnd = hwnd;
    CreateFloatControls(app);
    app->LayoutFloatingWindow();
    app->UpdateFloatingWindow(true);
    if (app->floatCompact) ApplyFloatRegion(hwnd, width, height, 1.0);
    app->floatSnapEdge = 0;
    if (app->floatCompact) {
        RECT workRect{};
        NearEdge(app, app->floatSnapEdge, workRect);
    }
    return hwnd;
}

void App::LayoutFloatingWindow() { ApplyFloatLayout(this); }

void App::UpdateFloatingWindow(bool artwork) {
    if (!floatWnd) return;
    UpdateFloatText(this);
    if (artwork) UpdateFloatCover();
}

void App::UpdateFloatCover() {
    if (!floatCover) return;
    HBITMAP next = CreateCoverBitmap(kFloatCoverPixels);
    if (floatCoverBitmap) DeleteObject(floatCoverBitmap);
    floatCoverBitmap = next;
    InvalidateRect(floatCover, nullptr, TRUE);
}

void App::SetFloatCompact(bool compact, bool persist) {
    if (!floatWnd) {
        floatCompact = compact;
        if (compact) floatDocked = true;
        settings.Data().floatCompact = floatDocked;
        if (persist) settings.Save();
        return;
    }
    if (floatAnimating) return;
    if (compact) floatDocked = true;
    RECT current{};
    GetWindowRect(floatWnd, &current);
    HMONITOR monitor = MonitorFromRect(&current, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return;
    RECT work = info.rcWork;
    int width = compact ? kFloatCompactSize : kFloatExpandedWidth;
    int height = compact ? kFloatCompactSize : kFloatExpandedHeight;
    int x = current.left;
    int y = current.top;
    if (compact) {
        if (floatSnapEdge & 1) x = work.left;
        else if (floatSnapEdge & 2) x = work.right - width;
        if (floatSnapEdge & 4) y = work.top;
        else if (floatSnapEdge & 8) y = work.bottom - height;
    } else {
        if (floatSnapEdge & 2) x = current.right - width;
        if (floatSnapEdge & 8) y = current.bottom - height;
    }
    x = std::max(static_cast<int>(work.left),
                 std::min(x, static_cast<int>(work.right) - width));
    y = std::max(static_cast<int>(work.top),
                 std::min(y, static_cast<int>(work.bottom) - height));
    RECT target{x, y, x + width, y + height};
    BeginFloatAnimation(this, compact, persist, target);
}

void App::SaveFloatPosition() {
    if (!floatWnd) return;
    RECT rect{};
    GetWindowRect(floatWnd, &rect);
    settings.Data().hasFloatPos = true;
    settings.Data().floatX = rect.left;
    settings.Data().floatY = rect.top;
    settings.Data().floatCompact = floatDocked;
    settings.Save();
}

void App::ResetFloatingPosition() {
    if (!floatWnd) return;
    StopFloatHoverTimers(this);
    if (floatAnimating) {
        KillTimer(floatWnd, kFloatAnimTimer);
        floatAnimating = false;
    }
    SetWindowRgn(floatWnd, nullptr, TRUE);
    floatCompact = false;
    floatDocked = false;
    floatSnapEdge = 0;
    int width = kFloatExpandedWidth;
    int height = kFloatExpandedHeight;
    POINT cursor{};
    GetCursorPos(&cursor);
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &info))
        work = info.rcWork;
    int x = work.right - width - 24;
    int y = work.bottom - height - 24;
    SetWindowPos(floatWnd, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE);
    LayoutFloatingWindow();
    UpdateFloatingWindow(true);
    SaveFloatPosition();
}

void FloatWindowRefresh(App* app) {
    if (app && app->floatWnd) {
        app->LayoutFloatingWindow();
        app->UpdateFloatingWindow(true);
    }
}

}  // namespace mk
