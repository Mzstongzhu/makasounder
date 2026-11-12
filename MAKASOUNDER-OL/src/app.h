#pragma once

#include <windows.h>

#include <gdiplus.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "artwork.h"
#include "association.h"
#include "audio.h"
#include "lyrics.h"
#include "metadata.h"
#include "online.h"
#include "resource.h"
#include "settings.h"

namespace mk {

constexpr wchar_t kMainWindowClass[] = L"MAKASOUNDER.MainWindow";
constexpr wchar_t kFloatWindowClass[] = L"MAKASOUNDER.FloatingWindow";
constexpr wchar_t kControllerClass[] = L"MAKASOUNDER.Controller";

struct Track {
    std::wstring path;
    TrackInfo info;
};

struct MakerLine {
    int ms = -1;
    std::wstring text;
};

class App {
public:
    App() = default;
    ~App();

    int Run(HINSTANCE instance, bool startFloat, const std::vector<std::wstring>& startupFiles);
    void RequestQuit();
    void OnTimer();

    void CreateMainWindow();
    void ShowMainWindow();
    void HideMainWindow();
    void CreateFloatingWindow();
    void ShowFloatingWindow();
    void HideFloatingWindow();
    void ToggleFloatingFromMain();
    void ReturnToMainFromFloat();
    void ResetFloatingPosition();
    void SaveFloatPosition();
    void SetFloatCompact(bool compact, bool persist);

    void AddFiles(const std::vector<std::wstring>& paths, bool showResult);
    void AddFilesFromDialog();
    void AddFolderFromDialog();
    void AutoScanWorkingDirectory(bool userInitiated = false);
    void RemoveTrack(int index);
    void ClearPlaylist();
    void PlaySelected();
    void PlayIndex(int index, bool autoplay);
    void TogglePlayback();
    void PlayNext(bool manual);
    void PlayPrev();
    void SeekTo(double seconds);
    void SetVolume(int percent);
    void SaveSettings();
    void CommitWindowPlacement();

    void RefreshAll(bool artwork);
    void RefreshPlaylist();
    void RefreshNowPlaying(bool artwork);
    void RefreshLyrics(bool rebuild);
    void RefreshTransport();
    void RefreshLyricSelection();
    void UpdateStatus();
    void UpdateWindowTitle();
    void LayoutMainWindow();
    void LayoutFloatingWindow();
    void UpdateFloatingWindow(bool artwork);

    void OpenSettings(HWND owner);
    void ApplyAssociation(bool enable, bool floatMode, HWND owner);
    void ShowAbout(HWND owner);
    void OpenLyricMaker(HWND owner);
    bool SaveMakerLrc(HWND owner);

    // Online functionality
    void OpenOnlineSearch(HWND owner);
    void AddOnlineTracks(const std::vector<OnlineTrack>& tracks);
    void DownloadAndPlayOnlineTrack(const OnlineTrack& track);
    void OpenServerSettings(HWND owner);
    void ApplyServerSettings();

    HBITMAP CreateCoverBitmap(int size) const;
    void ReleaseCoverBitmaps();
    void UpdateMainCover();
    void UpdateFloatCover();

    Track* CurrentTrack();
    const Track* CurrentTrack() const;
    std::wstring CurrentTitle() const;
    std::wstring CurrentArtist() const;
    std::wstring CurrentAlbum() const;

    HINSTANCE instance = nullptr;
    HWND mainWnd = nullptr;
    HWND floatWnd = nullptr;
    HWND controllerWnd = nullptr;

    AudioEngine audio;
    SettingsStore settings;
    TagReader tagReader;
    LyricService lyrics;
    ArtworkService artwork;
    OnlineService online;
    std::shared_ptr<Gdiplus::Bitmap> currentArtwork;

    std::vector<Track> playlist;
    int currentIndex = -1;
    int selectedIndex = -1;
    double position = 0.0;
    double duration = 0.0;
    int volume = 70;
    bool userSeeking = false;
    bool suppressSeekEvent = false;
    bool floatCompact = false;
    int floatSnapEdge = 0;
    bool floatAnimating = false;
    bool floatAnimTargetCompact = false;
    bool floatAnimPersist = false;
    bool floatAnimStartCompact = false;
    DWORD floatAnimStartTick = 0;
    RECT floatAnimStartRect{};
    RECT floatAnimEndRect{};
    bool floatBallPressed = false;
    bool floatBallMoved = false;
    POINT floatBallStartPoint{};
    RECT floatBallStartRect{};
    int floatBallOffsetX = 0;
    int floatBallOffsetY = 0;
    bool floatDocked = false;
    bool floatHoverPending = false;
    bool floatDockWatchActive = false;
    int floatOutsideChecks = 0;
    bool floatMenuOpen = false;

    HWND listView = nullptr;
    HWND lyricsList = nullptr;
    HWND coverStatic = nullptr;
    HWND seekBar = nullptr;
    HWND volumeBar = nullptr;
    HWND titleLabel = nullptr;
    HWND artistLabel = nullptr;
    HWND albumLabel = nullptr;
    HWND timeLabel = nullptr;
    HWND volumeLabel = nullptr;
    HWND lyricsSourceLabel = nullptr;
    HWND playButton = nullptr;
    HWND prevButton = nullptr;
    HWND nextButton = nullptr;
    HWND settingsButton = nullptr;
    HWND floatButton = nullptr;
    HWND statusBar = nullptr;

    HWND floatCover = nullptr;
    HWND floatTitle = nullptr;
    HWND floatArtist = nullptr;
    HWND floatAlbum = nullptr;
    HWND floatLyric = nullptr;
    HWND floatPrev = nullptr;
    HWND floatPlay = nullptr;
    HWND floatNext = nullptr;
    HWND floatInfo = nullptr;

    HBITMAP coverBitmap = nullptr;
    HBITMAP floatCoverBitmap = nullptr;
    HFONT uiFont = nullptr;
    HFONT titleFont = nullptr;

    std::wstring statusText = L"就绪";
    int lastPositionSec = -1;
    int lastDurationSec = -1;
    int lastLyricIndex = -2;
    bool updatingControls = false;

    std::vector<MakerLine> makerLines;
    int makerSelected = -1;
    std::vector<std::pair<int, int>> makerUndo;
    bool lyricMakerOpen = false;
    bool makerSeeking = false;

private:
    void SaveWindowPlacement();
    bool quitting_ = false;
    bool oleInitialized_ = false;
    ULONG_PTR gdiplusToken_ = 0;
    int timerTicks_ = 0;
};

HWND MainWindowCreate(App* app);
HWND FloatWindowCreate(App* app);
void MainWindowRefresh(App* app);
void FloatWindowRefresh(App* app);
bool HandleMediaKeys(App* app, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MakerDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ServerDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SearchDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace mk
