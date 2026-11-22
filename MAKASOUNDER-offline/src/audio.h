#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

namespace mk {

struct DirectShowState;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    bool Initialize(HWND window);
    void Shutdown();

    bool Open(const std::wstring& path, std::wstring& error);
    void Close();

    bool Play();
    bool Pause();
    bool TogglePlayPause();
    bool Seek(double seconds);

    bool IsLoaded() const { return bassHandle_ != 0 || mciOpen_ || dshow_ != nullptr; }
    bool IsPlaying() const { return loadedPlaying_; }
    bool IsPaused() const { return paused_; }
    double Position() const;
    double Duration() const;
    void SetVolume(int percent);
    int Volume() const { return volume_; }

    bool ConsumeEnded();
    std::wstring BackendName() const;
    std::wstring LastError() const { return lastError_; }

private:
    struct BassApi {
        HMODULE module = nullptr;
        using InitFn = BOOL(__stdcall*)(int, DWORD, DWORD, HWND, void*);
        using FreeFn = BOOL(__stdcall*)();
        using CreateFn = DWORD(__stdcall*)(BOOL, const void*, unsigned long long, unsigned long long, DWORD);
        using PlayFn = BOOL(__stdcall*)(DWORD, BOOL);
        using PauseFn = BOOL(__stdcall*)(DWORD);
        using StopFn = BOOL(__stdcall*)(DWORD);
        using SetPosFn = BOOL(__stdcall*)(DWORD, unsigned long long, DWORD);
        using GetPosFn = unsigned long long(__stdcall*)(DWORD, DWORD);
        using GetLenFn = unsigned long long(__stdcall*)(DWORD, DWORD);
        using Bytes2SecFn = double(__stdcall*)(DWORD, unsigned long long);
        using Sec2BytesFn = unsigned long long(__stdcall*)(DWORD, double);
        using SetAttrFn = BOOL(__stdcall*)(DWORD, DWORD, float);
        using ActiveFn = DWORD(__stdcall*)(DWORD);
        using StreamFreeFn = BOOL(__stdcall*)(DWORD);
        using PluginLoadFn = DWORD(__stdcall*)(const void*, DWORD);
        using PluginFreeFn = BOOL(__stdcall*)(DWORD);
        using ErrorFn = int(__stdcall*)();

        InitFn init = nullptr;
        FreeFn free = nullptr;
        CreateFn createFile = nullptr;
        PlayFn play = nullptr;
        PauseFn pause = nullptr;
        StopFn stop = nullptr;
        SetPosFn setPosition = nullptr;
        GetPosFn getPosition = nullptr;
        GetLenFn getLength = nullptr;
        Bytes2SecFn bytesToSeconds = nullptr;
        Sec2BytesFn secondsToBytes = nullptr;
        SetAttrFn setAttribute = nullptr;
        ActiveFn isActive = nullptr;
        StreamFreeFn streamFree = nullptr;
        PluginLoadFn pluginLoad = nullptr;
        PluginFreeFn pluginFree = nullptr;
        ErrorFn error = nullptr;
    };

    bool LoadBass();
    void UnloadBass();
    bool OpenBass(const std::wstring& path, std::wstring& error);
    bool OpenMci(const std::wstring& path, std::wstring& error);
    bool OpenDirectShow(const std::wstring& path, std::wstring& error);
    bool MakeCleanMp3(const std::wstring& path, std::wstring& cleanPath,
                      std::wstring& error);
    void RemoveTempFile();
    void CloseDirectShow();
    bool DShowPlay();
    bool DShowPause();
    bool DShowSeek(double seconds);
    double DShowPosition() const;
    double DShowDuration() const;
    void DShowSetVolume(int percent);
    bool DShowConsumeEnded();
    void CloseMci();
    double MciPosition() const;
    double MciLength() const;
    std::wstring MciMode() const;
    std::wstring BassErrorText() const;
    void UpdatePlayingState();

    BassApi bass_;
    bool bassInitialized_ = false;
    DWORD bassHandle_ = 0;
    std::vector<DWORD> plugins_;

    bool mciOpen_ = false;
    bool mciStarted_ = false;
    bool mciPaused_ = false;

    bool loadedPlaying_ = false;
    bool paused_ = false;
    bool started_ = false;
    bool endedPending_ = false;
    int volume_ = 70;
    double suppressEndUntil_ = 0.0;
    mutable std::wstring lastError_;
    bool backendBass_ = false;
    std::wstring mciAlias_ = L"MAKASOUNDER_AUDIO";
    std::wstring tempPath_;
    std::unique_ptr<DirectShowState> dshow_;
};

}  // namespace mk
