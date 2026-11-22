#include "audio.h"

#include "common.h"

#include <evcode.h>
#include <oaidl.h>
#include <objbase.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cwchar>

// Dev-C++'s MinGW ships partial DirectShow headers: it has strmiids/quartz
// libraries and some constants, but not the IGraphBuilder interfaces.  The
// small declarations below describe only the vtable slots this player uses.
// They are ABI-compatible with the system DirectShow interfaces.
extern "C" {
extern const GUID CLSID_FilterGraph;
extern const GUID IID_IGraphBuilder;
extern const GUID IID_IMediaControl;
extern const GUID IID_IMediaSeeking;
extern const GUID IID_IMediaEvent;
extern const GUID IID_IBasicAudio;
extern const GUID TIME_FORMAT_MEDIA_TIME;
}

constexpr DWORD AM_SEEKING_NoPositioning = 0;
constexpr DWORD AM_SEEKING_AbsolutePositioning = 1;

struct IFilterGraph : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE AddFilter(void*, LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveFilter(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnumFilters(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindFilterByName(LPCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ConnectDirect(void*, void*, const void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Reconnect(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Disconnect(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultSyncSource() = 0;
};

struct IGraphBuilder : public IFilterGraph {
    virtual HRESULT STDMETHODCALLTYPE Connect(void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Render(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE RenderFile(LPCWSTR, LPCWSTR) = 0;
};

struct IMediaControl : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE get_FilterGraph(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE Run() = 0;
    virtual HRESULT STDMETHODCALLTYPE Pause() = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
};

struct IMediaSeeking : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CheckCapabilities(DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsFormatSupported(const GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryPreferredFormat(GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTimeFormat(GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsUsingTimeFormat(const GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetTimeFormat(const GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDuration(LONGLONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStopPosition(LONGLONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPositions(const LONGLONG*, DWORD, const LONGLONG*,
                                                   DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition(LONGLONG*) = 0;
};

struct IMediaEvent : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetEvent(long*, LONG_PTR*, LONG_PTR*, long) = 0;
    virtual HRESULT STDMETHODCALLTYPE WaitForCompletion(long, long*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CancelDefaultHandling(long) = 0;
    virtual HRESULT STDMETHODCALLTYPE RestoreDefaultHandling(long) = 0;
    virtual HRESULT STDMETHODCALLTYPE FreeEventParams(long, LONG_PTR, LONG_PTR) = 0;
};

struct IBasicAudio : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE put_Volume(long) = 0;
};

namespace mk {

struct DirectShowState {
    IGraphBuilder* graph = nullptr;
    IMediaControl* control = nullptr;
    IMediaSeeking* seeking = nullptr;
    IMediaEvent* events = nullptr;
    IBasicAudio* audio = nullptr;
};

namespace {

constexpr DWORD kBassUnicode = 0x80000000u;
constexpr DWORD kBassPosByte = 0;
constexpr DWORD kBassAttribVolume = 2;
constexpr DWORD kBassActiveStopped = 0;
constexpr DWORD kBassActivePlaying = 1;
constexpr DWORD kBassActivePaused = 2;

std::wstring IntToWString(int value) {
    wchar_t buffer[32];
    _snwprintf(buffer, 32, L"%d", value);
    buffer[31] = 0;
    return buffer;
}

bool MciSend(const std::wstring& command, std::wstring& error, std::wstring* output = nullptr) {
    wchar_t buffer[2048] = {};
    MCIERROR result = mciSendStringW(command.c_str(), buffer, 2048, nullptr);
    if (result != 0) {
        error = MciErrorMessage(result);
        return false;
    }
    if (output) *output = buffer;
    return true;
}

template <typename T>
void Resolve(HMODULE module, const char* name, T& target) {
    target = reinterpret_cast<T>(GetProcAddress(module, name));
}

std::wstring HResultText(HRESULT result) {
    std::wstring text = ErrorMessage(static_cast<DWORD>(result));
    wchar_t code[24];
    _snwprintf(code, 24, L" (0x%08lX)", static_cast<unsigned long>(result));
    code[23] = 0;
    text += code;
    return text;
}

void ReleaseDirectShowState(DirectShowState* state) {
    if (!state) return;
    if (state->control) state->control->Stop();
    if (state->events) state->events->Release();
    if (state->audio) state->audio->Release();
    if (state->seeking) state->seeking->Release();
    if (state->control) state->control->Release();
    if (state->graph) state->graph->Release();
    delete state;
}

}  // namespace

AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() {
    CloseDirectShow();
    RemoveTempFile();
}

bool AudioEngine::LoadBass() {
    if (bass_.module) return true;
    std::wstring local = JoinPath(GetExeDir(), L"bass.dll");
    HMODULE module = LoadLibraryW(local.c_str());
    if (!module) module = LoadLibraryW(L"bass.dll");
    if (!module) return false;

    BassApi api;
    api.module = module;
    Resolve(module, "BASS_Init", api.init);
    Resolve(module, "BASS_Free", api.free);
    Resolve(module, "BASS_StreamCreateFile", api.createFile);
    Resolve(module, "BASS_ChannelPlay", api.play);
    Resolve(module, "BASS_ChannelPause", api.pause);
    Resolve(module, "BASS_ChannelStop", api.stop);
    Resolve(module, "BASS_ChannelSetPosition", api.setPosition);
    Resolve(module, "BASS_ChannelGetPosition", api.getPosition);
    Resolve(module, "BASS_ChannelGetLength", api.getLength);
    Resolve(module, "BASS_ChannelBytes2Seconds", api.bytesToSeconds);
    Resolve(module, "BASS_ChannelSeconds2Bytes", api.secondsToBytes);
    Resolve(module, "BASS_ChannelSetAttribute", api.setAttribute);
    Resolve(module, "BASS_ChannelIsActive", api.isActive);
    Resolve(module, "BASS_StreamFree", api.streamFree);
    Resolve(module, "BASS_PluginLoad", api.pluginLoad);
    Resolve(module, "BASS_PluginFree", api.pluginFree);
    Resolve(module, "BASS_ErrorGetCode", api.error);

    if (!api.init || !api.free || !api.createFile || !api.play || !api.pause || !api.setPosition ||
        !api.getPosition || !api.getLength || !api.bytesToSeconds || !api.secondsToBytes ||
        !api.setAttribute || !api.isActive || !api.streamFree || !api.error) {
        FreeLibrary(module);
        return false;
    }
    bass_ = api;
    return true;
}

void AudioEngine::UnloadBass() {
    if (bass_.module) {
        FreeLibrary(bass_.module);
        bass_.module = nullptr;
    }
    bass_ = BassApi{};
}

bool AudioEngine::Initialize(HWND window) {
    lastError_.clear();
    mciAlias_ = L"MAKASOUNDER_AUDIO_" + IntToWString(static_cast<int>(GetCurrentProcessId()));
    if (LoadBass()) {
        if (bass_.init(-1, 44100, 0, window, nullptr)) {
            bassInitialized_ = true;
            if (bass_.pluginLoad) {
                std::wstring pattern = JoinPath(GetExeDir(), L"bass*.dll");
                WIN32_FIND_DATAW data{};
                HANDLE find = FindFirstFileW(pattern.c_str(), &data);
                if (find != INVALID_HANDLE_VALUE) {
                    do {
                        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        if (IEquals(data.cFileName, L"bass.dll")) continue;
                        std::wstring pluginPath = JoinPath(GetExeDir(), data.cFileName);
                        DWORD plugin = bass_.pluginLoad(pluginPath.c_str(), kBassUnicode);
                        if (plugin) plugins_.push_back(plugin);
                    } while (FindNextFileW(find, &data));
                    FindClose(find);
                }
            }
            return true;
        }
        lastError_ = L"BASS 初始化失败";
        if (bass_.free) bass_.free();
        bassInitialized_ = false;
    }
    // MCI is always available as a compatibility path. Metadata still works without BASS.
    return true;
}

void AudioEngine::Shutdown() {
    Close();
    if (bassInitialized_) {
        for (DWORD plugin : plugins_) {
            if (bass_.pluginFree) bass_.pluginFree(plugin);
        }
        plugins_.clear();
        if (bass_.free) bass_.free();
        bassInitialized_ = false;
    }
    UnloadBass();
}

std::wstring AudioEngine::BassErrorText() const {
    if (!bass_.error) return L"BASS 错误";
    int code = bass_.error();
    std::wstring text = L"BASS 错误 ";
    wchar_t number[16];
    _snwprintf(number, 16, L"%d", code);
    number[15] = 0;
    text += number;
    switch (code) {
        case 2: text += L"（文件无法打开）"; break;
        case 3: text += L"（驱动不可用）"; break;
        case 6: text += L"（格式不支持）"; break;
        case 8: text += L"（内存不足）"; break;
        case 12: text += L"（音频设备不可用）"; break;
        case 41: text += L"（解码器未加载）"; break;
        default: break;
    }
    return text;
}

bool AudioEngine::OpenBass(const std::wstring& path, std::wstring& error) {
    if (!bassInitialized_ || !bass_.createFile) {
        error = L"BASS 未初始化";
        return false;
    }
    bassHandle_ = bass_.createFile(FALSE, path.c_str(), 0, 0, kBassUnicode);
    if (!bassHandle_) {
        error = BassErrorText();
        return false;
    }
    backendBass_ = true;
    bass_.setAttribute(bassHandle_, kBassAttribVolume, static_cast<float>(volume_) / 100.0f);
    return true;
}

void AudioEngine::CloseMci() {
    if (mciOpen_) {
        std::wstring ignore;
        MciSend(std::wstring(L"close ") + mciAlias_, ignore);
    }
    mciOpen_ = false;
    mciStarted_ = false;
    mciPaused_ = false;
}

bool AudioEngine::OpenMci(const std::wstring& path, std::wstring& error) {
    CloseMci();
    // MCI is an old ANSI-oriented API; the short path is the most reliable form
    // for non-ASCII file names and paths with spaces.
    std::wstring escapedPath = path;
    wchar_t shortPath[32768] = {};
    if (GetShortPathNameW(path.c_str(), shortPath,
                          static_cast<DWORD>(sizeof(shortPath) / sizeof(shortPath[0]))) > 0) {
        escapedPath = shortPath;
    }
    std::wstring open = L"open \"" + escapedPath + L"\" alias " + mciAlias_;
    std::wstring detail;
    bool ok = MciSend(open, detail);
    if (!ok) {
        // Windows' MCI filter for MP3/AAC/WMA is usually "mpegvideo".
        std::wstring typed = L"open \"" + escapedPath + L"\" type mpegvideo alias " + mciAlias_;
        ok = MciSend(typed, detail);
    }
    if (!ok) {
        error = L"MCI 无法打开该文件：" + detail;
        return false;
    }
    mciOpen_ = true;
    mciStarted_ = false;
    mciPaused_ = false;
    std::wstring timeFormatError;
    MciSend(std::wstring(L"set ") + mciAlias_ + L" time format milliseconds", timeFormatError);
    std::wstring volumeCommand = std::wstring(L"setaudio ") + mciAlias_ + L" volume to " +
                                 IntToWString(volume_ * 10);
    MciSend(volumeCommand, timeFormatError);
    backendBass_ = false;
    return true;
}

bool AudioEngine::OpenDirectShow(const std::wstring& path, std::wstring& error) {
    CloseDirectShow();
    auto state = std::make_unique<DirectShowState>();
    HRESULT result = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_IGraphBuilder,
                                      reinterpret_cast<void**>(&state->graph));
    if (FAILED(result) || !state->graph) {
        error = L"DirectShow 初始化失败：" + HResultText(result);
        ReleaseDirectShowState(state.release());
        return false;
    }
    result = state->graph->RenderFile(path.c_str(), nullptr);
    if (FAILED(result)) {
        error = L"DirectShow 无法打开该文件：" + HResultText(result);
        ReleaseDirectShowState(state.release());
        return false;
    }
    if (FAILED(state->graph->QueryInterface(IID_IMediaControl,
                                            reinterpret_cast<void**>(&state->control))) ||
        FAILED(state->graph->QueryInterface(IID_IMediaSeeking,
                                            reinterpret_cast<void**>(&state->seeking))) ||
        FAILED(state->graph->QueryInterface(IID_IMediaEvent,
                                            reinterpret_cast<void**>(&state->events)))) {
        error = L"DirectShow 无法获取播放控制接口。";
        ReleaseDirectShowState(state.release());
        return false;
    }
    if (FAILED(state->graph->QueryInterface(IID_IBasicAudio,
                                            reinterpret_cast<void**>(&state->audio)))) {
        state->audio = nullptr;
    }
    state->seeking->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
    if (state->audio) {
        LONG volume = volume_ <= 0 ? -10000
                                   : (volume_ >= 100
                                          ? 0
                                          : static_cast<LONG>(2000.0 *
                                                              std::log10(volume_ / 100.0)));
        state->audio->put_Volume(volume);
    }
    dshow_ = std::move(state);
    backendBass_ = false;
    mciOpen_ = false;
    return true;
}

void AudioEngine::CloseDirectShow() {
    if (dshow_) {
        ReleaseDirectShowState(dshow_.release());
    }
}

void AudioEngine::RemoveTempFile() {
    if (!tempPath_.empty()) {
        DeleteFileW(tempPath_.c_str());
        tempPath_.clear();
    }
}

bool AudioEngine::MakeCleanMp3(const std::wstring& path, std::wstring& cleanPath,
                               std::wstring& error) {
    cleanPath.clear();
    HANDLE input = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE) {
        error = L"无法读取源文件";
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(input, &size) || size.QuadPart < 32) {
        CloseHandle(input);
        error = L"文件太小";
        return false;
    }
    uint8_t header[10] = {};
    DWORD read = 0;
    if (!ReadFile(input, header, 10, &read, nullptr) || read != 10 ||
        header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        CloseHandle(input);
        error = L"不是带 ID3 标签的 MP3";
        return false;
    }
    uint64_t tagSize = (static_cast<uint64_t>(header[6] & 0x7F) << 21) |
                       (static_cast<uint64_t>(header[7] & 0x7F) << 14) |
                       (static_cast<uint64_t>(header[8] & 0x7F) << 7) |
                       static_cast<uint64_t>(header[9] & 0x7F);
    uint64_t fileSize = static_cast<uint64_t>(size.QuadPart);
    uint64_t audioStart = 10 + tagSize;
    if (audioStart >= fileSize) {
        CloseHandle(input);
        error = L"ID3 标签超出文件范围";
        return false;
    }

    // Skip a possible ID3v2.4 footer/padding by locating the first MPEG frame.
    std::vector<uint8_t> scan(64 * 1024);
    LARGE_INTEGER seek{};
    seek.QuadPart = static_cast<LONGLONG>(audioStart);
    SetFilePointerEx(input, seek, nullptr, FILE_BEGIN);
    DWORD scanRead = 0;
    ReadFile(input, scan.data(), static_cast<DWORD>(scan.size()), &scanRead, nullptr);
    uint64_t found = 0;
    for (DWORD i = 0; i + 4 <= scanRead; ++i) {
        if (scan[i] == 0xFF && (scan[i + 1] & 0xE0) == 0xE0) {
            int version = (scan[i + 1] >> 3) & 3;
            int layer = (scan[i + 1] >> 1) & 3;
            int bitrate = (scan[i + 2] >> 4) & 0x0F;
            int rate = (scan[i + 2] >> 2) & 3;
            if (version != 1 && layer != 0 && bitrate != 0 && bitrate != 15 && rate != 3) {
                found = audioStart + i;
                break;
            }
        }
    }
    if (!found) {
        CloseHandle(input);
        error = L"没有找到有效的 MPEG 音频帧";
        return false;
    }
    uint64_t audioEnd = fileSize;
    if (audioEnd >= 128) {
        uint8_t tag[3] = {};
        LARGE_INTEGER tail{};
        tail.QuadPart = static_cast<LONGLONG>(audioEnd - 128);
        if (SetFilePointerEx(input, tail, nullptr, FILE_BEGIN) &&
            ReadFile(input, tag, 3, &read, nullptr) && read == 3 && tag[0] == 'T' &&
            tag[1] == 'A' && tag[2] == 'G') {
            audioEnd -= 128;
        }
    }
    if (audioEnd <= found + 16) {
        CloseHandle(input);
        error = L"音频数据为空";
        return false;
    }

    wchar_t tempDir[MAX_PATH] = {};
    DWORD tempLength = GetTempPathW(MAX_PATH, tempDir);
    if (tempLength == 0 || tempLength >= MAX_PATH) {
        CloseHandle(input);
        error = L"无法获取临时目录";
        return false;
    }
    std::wstring baseDir = JoinPath(tempDir, L"MAKASOUNDER");
    CreateDirectoryW(baseDir.c_str(), nullptr);
    wchar_t name[128];
    for (int attempt = 0; attempt < 100; ++attempt) {
        _snwprintf(name, 128, L"mks_%lu_%lu_%d.mp3", GetCurrentProcessId(), GetTickCount(),
                   attempt);
        name[127] = 0;
        cleanPath = JoinPath(baseDir, name);
        if (GetFileAttributesW(cleanPath.c_str()) == INVALID_FILE_ATTRIBUTES) break;
        cleanPath.clear();
    }
    if (cleanPath.empty()) {
        CloseHandle(input);
        error = L"无法创建临时文件名";
        return false;
    }
    HANDLE output = CreateFileW(cleanPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        CloseHandle(input);
        cleanPath.clear();
        error = L"无法创建临时文件";
        return false;
    }
    seek.QuadPart = static_cast<LONGLONG>(found);
    SetFilePointerEx(input, seek, nullptr, FILE_BEGIN);
    uint64_t remaining = audioEnd - found;
    std::vector<uint8_t> buffer(1024 * 1024);
    bool ok = true;
    while (remaining > 0) {
        DWORD chunk = static_cast<DWORD>(std::min<uint64_t>(remaining, buffer.size()));
        DWORD got = 0;
        DWORD written = 0;
        if (!ReadFile(input, buffer.data(), chunk, &got, nullptr) || got == 0 ||
            !WriteFile(output, buffer.data(), got, &written, nullptr) || written != got) {
            ok = false;
            break;
        }
        remaining -= got;
    }
    CloseHandle(output);
    CloseHandle(input);
    if (!ok) {
        DeleteFileW(cleanPath.c_str());
        cleanPath.clear();
        error = L"写入临时文件失败";
        return false;
    }
    return true;
}

bool AudioEngine::DShowPlay() {
    if (!dshow_ || !dshow_->control) return false;
    if (FAILED(dshow_->control->Run())) return false;
    paused_ = false;
    started_ = true;
    loadedPlaying_ = true;
    endedPending_ = false;
    suppressEndUntil_ = NowSeconds() + 0.45;
    return true;
}

bool AudioEngine::DShowPause() {
    if (!dshow_ || !dshow_->control) return false;
    if (FAILED(dshow_->control->Pause())) return false;
    paused_ = true;
    loadedPlaying_ = false;
    return true;
}

bool AudioEngine::DShowSeek(double seconds) {
    if (!dshow_ || !dshow_->seeking) return false;
    if (dshow_->events) {
        long code = 0;
        LONG_PTR p1 = 0;
        LONG_PTR p2 = 0;
        while (SUCCEEDED(dshow_->events->GetEvent(&code, &p1, &p2, 0))) {
            dshow_->events->FreeEventParams(code, p1, p2);
        }
    }
    LONGLONG target = static_cast<LONGLONG>(seconds * 10000000.0);
    HRESULT result = dshow_->seeking->SetPositions(
        &target, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
    if (FAILED(result)) return false;
    if (paused_) dshow_->control->Pause();
    else dshow_->control->Run();
    suppressEndUntil_ = NowSeconds() + 0.6;
    return true;
}

double AudioEngine::DShowPosition() const {
    if (!dshow_ || !dshow_->seeking) return 0.0;
    LONGLONG position = 0;
    if (FAILED(dshow_->seeking->GetCurrentPosition(&position))) return 0.0;
    return static_cast<double>(position) / 10000000.0;
}

double AudioEngine::DShowDuration() const {
    if (!dshow_ || !dshow_->seeking) return 0.0;
    LONGLONG duration = 0;
    if (FAILED(dshow_->seeking->GetDuration(&duration)) || duration <= 0) return 0.0;
    return static_cast<double>(duration) / 10000000.0;
}

void AudioEngine::DShowSetVolume(int percent) {
    if (!dshow_ || !dshow_->audio) return;
    LONG volume = percent <= 0 ? -10000
                               : (percent >= 100
                                      ? 0
                                      : static_cast<LONG>(2000.0 * std::log10(percent / 100.0)));
    dshow_->audio->put_Volume(volume);
}

bool AudioEngine::DShowConsumeEnded() {
    if (!dshow_ || !dshow_->events) return false;
    long code = 0;
    LONG_PTR p1 = 0;
    LONG_PTR p2 = 0;
    bool complete = false;
    while (SUCCEEDED(dshow_->events->GetEvent(&code, &p1, &p2, 0))) {
        dshow_->events->FreeEventParams(code, p1, p2);
        if (code == EC_COMPLETE || code == EC_ERRORABORT || code == EC_USERABORT) {
            complete = true;
        }
    }
    if (!complete) {
        double duration = DShowDuration();
        double position = DShowPosition();
        if (duration > 0.0 && position > 0.0 && position >= duration - 0.2) complete = true;
    }
    if (complete) {
        started_ = false;
        loadedPlaying_ = false;
        endedPending_ = true;
        return true;
    }
    return false;
}

bool AudioEngine::Open(const std::wstring& path, std::wstring& error) {
    Close();
    error.clear();
    lastError_.clear();
    auto resetState = [&] {
        started_ = false;
        paused_ = false;
        loadedPlaying_ = false;
        endedPending_ = false;
    };
    std::wstring bassError;
    if (bassInitialized_ && OpenBass(path, bassError)) {
        resetState();
        return true;
    }
    std::wstring mciError;
    if (OpenMci(path, mciError)) {
        resetState();
        return true;
    }

    // Some MP3 files have large or unusual ID3 blocks that trip up both MCI and
    // DirectShow.  A tag-stripped temporary copy usually plays normally.
    std::wstring cleanPath;
    std::wstring cleanError;
    std::wstring cleanAttemptError;
    bool hasCleanCopy = MakeCleanMp3(path, cleanPath, cleanError);
    if (hasCleanCopy) {
        tempPath_ = cleanPath;
        std::wstring cleanMciError;
        if (OpenMci(cleanPath, cleanMciError)) {
            resetState();
            return true;
        }
        cleanAttemptError = cleanMciError;
        std::wstring cleanDirectShowError;
        if (OpenDirectShow(cleanPath, cleanDirectShowError)) {
            resetState();
            return true;
        }
        if (!cleanDirectShowError.empty()) {
            cleanAttemptError += L" / " + cleanDirectShowError;
        }
        RemoveTempFile();
    }

    std::wstring directShowError;
    if (OpenDirectShow(path, directShowError)) {
        resetState();
        return true;
    }
    error = mciError;
    if (!cleanAttemptError.empty()) error += L" / 清理标签后：" + cleanAttemptError;
    if (!directShowError.empty()) error += L" / " + directShowError;
    if (!bassError.empty()) error = bassError + L" / " + error;
    lastError_ = error;
    RemoveTempFile();
    return false;
}

void AudioEngine::Close() {
    if (bassHandle_) {
        if (bass_.streamFree) bass_.streamFree(bassHandle_);
        bassHandle_ = 0;
    }
    CloseMci();
    CloseDirectShow();
    RemoveTempFile();
    backendBass_ = false;
    loadedPlaying_ = false;
    paused_ = false;
    started_ = false;
    endedPending_ = false;
}

bool AudioEngine::Play() {
    if (!IsLoaded()) return false;
    if (dshow_) {
        if (!DShowPlay()) {
            lastError_ = L"DirectShow 无法开始播放";
            return false;
        }
    } else if (backendBass_) {
        if (!bass_.play(bassHandle_, FALSE)) {
            lastError_ = BassErrorText();
            return false;
        }
    } else {
        std::wstring error;
        bool ok;
        if (mciPaused_) {
            ok = MciSend(std::wstring(L"resume ") + mciAlias_, error);
        } else {
            ok = MciSend(std::wstring(L"play ") + mciAlias_ + L" from 0", error);
        }
        if (!ok) {
            lastError_ = error;
            return false;
        }
        mciPaused_ = false;
        mciStarted_ = true;
    }
    paused_ = false;
    started_ = true;
    loadedPlaying_ = true;
    endedPending_ = false;
    suppressEndUntil_ = NowSeconds() + 0.45;
    return true;
}

bool AudioEngine::Pause() {
    if (!IsLoaded() || paused_) return false;
    if (dshow_) {
        if (!DShowPause()) return false;
    } else if (backendBass_) {
        if (!bass_.pause(bassHandle_)) return false;
    } else {
        std::wstring error;
        if (!MciSend(std::wstring(L"pause ") + mciAlias_, error)) {
            lastError_ = error;
            return false;
        }
        mciPaused_ = true;
    }
    paused_ = true;
    loadedPlaying_ = false;
    return true;
}

bool AudioEngine::TogglePlayPause() {
    if (!IsLoaded()) return false;
    if (paused_) return Play();
    if (loadedPlaying_) return Pause();
    return Play();
}

bool AudioEngine::Seek(double seconds) {
    if (!IsLoaded()) return false;
    double length = Duration();
    if (length > 0.0) seconds = std::max(0.0, std::min(seconds, length));
    if (seconds < 0.0) seconds = 0.0;
    if (dshow_) {
        if (!DShowSeek(seconds)) return false;
    } else if (backendBass_) {
        if (!bass_.setPosition(bassHandle_, bass_.secondsToBytes(bassHandle_, seconds), kBassPosByte))
            return false;
    } else {
        std::wstring error;
        int ms = static_cast<int>(seconds * 1000.0);
        if (!MciSend(std::wstring(L"seek ") + mciAlias_ + L" to " + IntToWString(ms), error)) {
            lastError_ = error;
            return false;
        }
        if (started_ && !paused_) {
            if (!MciSend(std::wstring(L"play ") + mciAlias_ + L" from " + IntToWString(ms), error)) {
                lastError_ = error;
                return false;
            }
        }
    }
    suppressEndUntil_ = NowSeconds() + 0.6;
    return true;
}

double AudioEngine::Position() const {
    if (dshow_) return DShowPosition();
    if (backendBass_ && bassHandle_) {
        unsigned long long pos = bass_.getPosition(bassHandle_, kBassPosByte);
        return bass_.bytesToSeconds(bassHandle_, pos);
    }
    if (mciOpen_) return MciPosition();
    return 0.0;
}

double AudioEngine::Duration() const {
    if (dshow_) return DShowDuration();
    if (backendBass_ && bassHandle_) {
        unsigned long long length = bass_.getLength(bassHandle_, kBassPosByte);
        return bass_.bytesToSeconds(bassHandle_, length);
    }
    if (mciOpen_) return MciLength();
    return 0.0;
}

void AudioEngine::SetVolume(int percent) {
    volume_ = ClampValue(percent, 0, 100);
    if (dshow_) {
        DShowSetVolume(volume_);
    } else if (backendBass_ && bassHandle_) {
        bass_.setAttribute(bassHandle_, kBassAttribVolume, static_cast<float>(volume_) / 100.0f);
    } else if (mciOpen_) {
        std::wstring error;
        MciSend(std::wstring(L"setaudio ") + mciAlias_ + L" volume to " +
                    IntToWString(volume_ * 10),
                error);
    }
}

std::wstring AudioEngine::MciMode() const {
    if (!mciOpen_) return {};
    wchar_t buffer[128] = {};
    std::wstring command = std::wstring(L"status ") + mciAlias_ + L" mode";
    if (mciSendStringW(command.c_str(), buffer, 128, nullptr) != 0) return {};
    return buffer;
}

double AudioEngine::MciPosition() const {
    wchar_t buffer[128] = {};
    std::wstring command = std::wstring(L"status ") + mciAlias_ + L" position";
    if (mciSendStringW(command.c_str(), buffer, 128, nullptr) != 0) return 0.0;
    return static_cast<double>(std::wcstol(buffer, nullptr, 10)) / 1000.0;
}

double AudioEngine::MciLength() const {
    wchar_t buffer[128] = {};
    std::wstring command = std::wstring(L"status ") + mciAlias_ + L" length";
    if (mciSendStringW(command.c_str(), buffer, 128, nullptr) != 0) return 0.0;
    return static_cast<double>(std::wcstol(buffer, nullptr, 10)) / 1000.0;
}

bool AudioEngine::ConsumeEnded() {
    if (!IsLoaded() || !started_ || paused_) return false;
    if (endedPending_) {
        endedPending_ = false;
        loadedPlaying_ = false;
        return true;
    }
    if (NowSeconds() < suppressEndUntil_) return false;
    if (dshow_) return DShowConsumeEnded();
    bool stopped = false;
    if (backendBass_) {
        DWORD active = bass_.isActive(bassHandle_);
        if (active == kBassActivePlaying) loadedPlaying_ = true;
        else if (active == kBassActivePaused) {
            loadedPlaying_ = false;
            paused_ = true;
            return false;
        } else if (active == kBassActiveStopped) {
            stopped = true;
        }
    } else {
        std::wstring mode = MciMode();
        if (mode == L"playing") {
            loadedPlaying_ = true;
        } else if (mode == L"paused") {
            loadedPlaying_ = false;
            paused_ = true;
            return false;
        } else if (mode == L"stopped") {
            stopped = true;
        }
    }
    if (stopped) {
        started_ = false;
        loadedPlaying_ = false;
        endedPending_ = true;
        return true;
    }
    return false;
}

std::wstring AudioEngine::BackendName() const {
    if (backendBass_) return L"BASS";
    if (dshow_) return L"DirectShow";
    if (mciOpen_) return L"MCI";
    return L"未连接";
}

}  // namespace mk
