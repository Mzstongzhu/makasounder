#include "app.h"

#include "common.h"
#include "online.h"
#include "resource.h"

#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>

#include <algorithm>
#include <string>
#include <vector>

namespace mk {

namespace {

std::wstring NumberText(long long value) {
    wchar_t buffer[32];
    _snwprintf(buffer, 32, L"%lld", value);
    buffer[31] = 0;
    return buffer;
}

std::vector<OnlineTrack> g_searchResults;
std::vector<OnlineTrack> g_libraryResults;
std::wstring g_uploadPath;

void FillTrackList(HWND list, const std::vector<OnlineTrack>& tracks) {
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& t : tracks) {
        std::wstring line = t.title + L"  ·  " + t.artist;
        if (!t.album.empty()) line += L"  ·  " + t.album;
        if (!t.duration.empty()) line += L"  (" + t.duration + L")";
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
}

int SelectedIndex(HWND list) {
    int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    return sel;
}

bool PromptText(HWND owner, const wchar_t* title, std::wstring& value) {
    std::wstring buffer(512, L'\0');
    // 简单输入：复用在线搜索框所在对话框前，直接用 Windows 输入不需要额外资源
    (void)owner;
    (void)title;
    (void)buffer;
    (void)value;
    return false;
}

}  // namespace

INT_PTR CALLBACK ServerDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch (message) {
        case WM_INITDIALOG: {
            app = reinterpret_cast<App*>(lParam);
            SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LPARAM>(app));
            SetDlgItemTextW(dialog, IDC_SERVER_URL, app->settings.Data().serverUrl.c_str());
            SetDlgItemTextW(dialog, IDC_SERVER_STATUS,
                            app->online.IsLoggedIn()
                                ? (L"已登录：" + app->online.Username()).c_str()
                                : L"未登录（公开服务器可直接搜索）");
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_SERVER_TEST) {
                wchar_t url[512] = {};
                GetDlgItemTextW(dialog, IDC_SERVER_URL, url, 512);
                app->online.SetServer(url);
                std::wstring error;
                if (app->online.TestConnection(error)) {
                    SetDlgItemTextW(dialog, IDC_SERVER_STATUS, L"连接成功");
                } else {
                    SetDlgItemTextW(dialog, IDC_SERVER_STATUS, (L"连接失败：" + error).c_str());
                }
                return TRUE;
            }
            if (id == IDC_SERVER_LOGIN || id == IDC_SERVER_REGISTER) {
                wchar_t url[512] = {}, user[128] = {}, pass[128] = {};
                GetDlgItemTextW(dialog, IDC_SERVER_URL, url, 512);
                GetDlgItemTextW(dialog, IDC_SERVER_USER, user, 128);
                GetDlgItemTextW(dialog, IDC_SERVER_PASS, pass, 128);
                app->online.SetServer(url);
                std::wstring error;
                bool ok = id == IDC_SERVER_LOGIN ? app->online.Login(user, pass, error)
                                                 : app->online.Register(user, pass, error);
                if (ok) {
                    app->settings.Data().serverUrl = url;
                    app->settings.Data().serverToken = app->online.Token();
                    app->settings.Data().serverUser = app->online.Username();
                    app->settings.Save();
                    SetDlgItemTextW(dialog, IDC_SERVER_STATUS,
                                    (L"已登录：" + app->online.Username()).c_str());
                    app->UpdateStatus();
                } else {
                    SetDlgItemTextW(dialog, IDC_SERVER_STATUS, (L"失败：" + error).c_str());
                }
                return TRUE;
            }
            if (id == IDC_SERVER_LOGOUT) {
                app->online.ClearToken();
                app->settings.Data().serverToken.clear();
                app->settings.Data().serverUser.clear();
                app->settings.Save();
                SetDlgItemTextW(dialog, IDC_SERVER_STATUS, L"已退出登录");
                return TRUE;
            }
            if (id == IDOK) {
                wchar_t url[512] = {};
                GetDlgItemTextW(dialog, IDC_SERVER_URL, url, 512);
                if (app->online.Server() != url) app->online.SetServer(url);
                app->settings.Data().serverUrl = url;
                app->settings.Save();
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

INT_PTR CALLBACK OnlineSearchDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch (message) {
        case WM_INITDIALOG: {
            app = reinterpret_cast<App*>(lParam);
            SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LPARAM>(app));
            g_searchResults.clear();
            SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, L"输入关键词搜索");
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_SEARCH_GO || (id == IDC_SEARCH_QUERY && HIWORD(wParam) == EN_CHANGE)) {
                if (id == IDC_SEARCH_QUERY) return TRUE;
                wchar_t query[256] = {};
                GetDlgItemTextW(dialog, IDC_SEARCH_QUERY, query, 256);
                if (!*query) return TRUE;
                if (!app->online.IsConfigured()) {
                    SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, L"请先在设置中配置服务器");
                    return TRUE;
                }
                SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, L"搜索中...");
                std::wstring error;
                g_searchResults = app->online.Search(query, error);
                FillTrackList(GetDlgItem(dialog, IDC_SEARCH_RESULTS), g_searchResults);
                SetDlgItemTextW(dialog, IDC_SEARCH_STATUS,
                                (L"找到 " + NumberText(static_cast<long long>(g_searchResults.size())) + L" 首").c_str());
                return TRUE;
            }
            if (id == IDC_SEARCH_PLAY) {
                int sel = SelectedIndex(GetDlgItem(dialog, IDC_SEARCH_RESULTS));
                if (sel < 0 || sel >= static_cast<int>(g_searchResults.size())) return TRUE;
                auto& t = g_searchResults[static_cast<size_t>(sel)];
                app->PlayOnlineTrack(app->online.Server() + t.streamUrl, t.title, t.artist);
                SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, (L"正在流式播放：" + t.title).c_str());
                return TRUE;
            }
            if (id == IDC_SEARCH_ADD || id == IDC_SEARCH_DOWNLOAD) {
                int sel = SelectedIndex(GetDlgItem(dialog, IDC_SEARCH_RESULTS));
                if (sel < 0 || sel >= static_cast<int>(g_searchResults.size())) return TRUE;
                auto& t = g_searchResults[static_cast<size_t>(sel)];
                std::wstring savePath, error;
                SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, L"下载中...");
                UpdateWindow(dialog);
                if (!app->online.DownloadTrack(t, savePath, error)) {
                    SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, (L"下载失败：" + error).c_str());
                    return TRUE;
                }
                app->AddFiles({savePath}, false);
                SetDlgItemTextW(dialog, IDC_SEARCH_STATUS, (L"已加入播放列表：" + t.title).c_str());
                app->UpdateStatus();
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

INT_PTR CALLBACK UploadDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch (message) {
        case WM_INITDIALOG:
            app = reinterpret_cast<App*>(lParam);
            SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LPARAM>(app));
            return TRUE;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_UPLOAD_BROWSE) {
                wchar_t file[MAX_PATH] = {};
                OPENFILENAMEW open{};
                open.lStructSize = sizeof(open);
                open.hwndOwner = dialog;
                open.lpstrFilter = L"音频\0*.mp3;*.flac;*.wav;*.m4a;*.ogg\0所有文件\0*.*\0\0";
                open.lpstrFile = file;
                open.nMaxFile = MAX_PATH;
                if (GetOpenFileNameW(&open)) {
                    g_uploadPath = file;
                    SetDlgItemTextW(dialog, IDC_UPLOAD_FILE, file);
                }
                return TRUE;
            }
            if (id == IDC_UPLOAD_SUBMIT) {
                if (g_uploadPath.empty()) {
                    SetDlgItemTextW(dialog, IDC_UPLOAD_STATUS, L"请先选择文件");
                    return TRUE;
                }
                if (!app->online.IsLoggedIn()) {
                    SetDlgItemTextW(dialog, IDC_UPLOAD_STATUS, L"请先登录");
                    return TRUE;
                }
                SetDlgItemTextW(dialog, IDC_UPLOAD_STATUS, L"上传中...");
                UpdateWindow(dialog);
                std::wstring error;
                if (app->online.UploadTrack(g_uploadPath, error)) {
                    SetDlgItemTextW(dialog, IDC_UPLOAD_STATUS, L"已提交，等待服主审核");
                } else {
                    SetDlgItemTextW(dialog, IDC_UPLOAD_STATUS, (L"上传失败：" + error).c_str());
                }
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

void App::ShowHomePage() {
    showingLibrary = false;
    if (libraryView) ShowWindow(libraryView, SW_HIDE);
    if (listView) ShowWindow(listView, SW_SHOW);
    if (pagePrev) ShowWindow(pagePrev, SW_HIDE);
    if (pageNext) ShowWindow(pageNext, SW_HIDE);
    if (pageLabel) ShowWindow(pageLabel, SW_HIDE);
    RefreshPlaylist();
}

void App::ShowLibraryPage() {
    if (!online.IsConfigured()) {
        ShowHomePage();
        OpenOnlineServer(mainWnd);
        return;
    }
    showingLibrary = true;
    if (!libraryView) {
        libraryView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
            0, 0, 10, 10, mainWnd, reinterpret_cast<HMENU>(IDC_LIBRARY_LIST), nullptr, nullptr);
        SendMessageW(libraryView, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
        ListView_SetExtendedListViewStyle(libraryView, LVS_EX_FULLROWSELECT | 0x00010000);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<wchar_t*>(L"歌曲");
        col.cx = 300;
        ListView_InsertColumn(libraryView, 0, &col);
        col.pszText = const_cast<wchar_t*>(L"艺术家");
        col.cx = 180;
        ListView_InsertColumn(libraryView, 1, &col);
        col.pszText = const_cast<wchar_t*>(L"时长");
        col.cx = 70;
        ListView_InsertColumn(libraryView, 2, &col);
    }
    if (listView) {
        RECT rc{};
        GetWindowRect(listView, &rc);
        MapWindowPoints(nullptr, mainWnd, reinterpret_cast<POINT*>(&rc), 2);
        ShowWindow(libraryView, SW_SHOW);
        SetWindowPos(libraryView, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER);
        ShowWindow(listView, SW_HIDE);
    }
    if (pagePrev) ShowWindow(pagePrev, SW_SHOW);
    if (pageNext) ShowWindow(pageNext, SW_SHOW);
    if (pageLabel) ShowWindow(pageLabel, SW_SHOW);
    LoadLibraryPage(0);
}

void App::LoadLibraryPage(int page) {
    std::wstring error;
    if (libraryTracks.empty()) {
        libraryTracks = online.Library(error);
        if (libraryTracks.empty()) {
            statusText = error.empty() ? L"曲库为空" : (L"加载曲库失败：" + error);
            UpdateStatus();
            return;
        }
        std::sort(libraryTracks.begin(), libraryTracks.end(),
                  [](const OnlineTrack& a, const OnlineTrack& b) { return a.title < b.title; });
    }
    const int pageSize = 100;
    int totalPages = (static_cast<int>(libraryTracks.size()) + pageSize - 1) / pageSize;
    if (totalPages < 1) totalPages = 1;
    if (page < 0) page = 0;
    if (page >= totalPages) page = totalPages - 1;
    libraryPage = page;
    int start = page * pageSize;
    int count = std::min(pageSize, static_cast<int>(libraryTracks.size()) - start);
    ListView_SetItemCountEx(libraryView, count > 0 ? count : 0, 0);
    if (pageLabel) {
        std::wstring label = L"第 " + NumberText(page + 1) + L" / " + NumberText(totalPages) + L" 页";
        SetWindowTextW(pageLabel, label.c_str());
    }
    statusText = L"服务器曲库共 " + NumberText(static_cast<long long>(libraryTracks.size())) +
                 L" 首，双击歌曲可直接在线播放";
    UpdateStatus();
}

}  // namespace mk
