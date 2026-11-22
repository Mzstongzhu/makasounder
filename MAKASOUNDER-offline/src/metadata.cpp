#include "metadata.h"

#include "common.h"
#include "fileio.h"
#include "lyrics.h"

#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cwctype>
#include <map>
#include <utility>

namespace mk {

namespace {

constexpr size_t kMaxTagRead = 48u * 1024u * 1024u;

std::wstring CleanTagText(std::wstring text) {
    for (wchar_t& c : text) {
        if (c == L'\0') c = L' ';
    }
    text = Trim(std::move(text));
    while (!text.empty() && text.back() == L'\0') text.pop_back();
    return text;
}

std::wstring DecodeId3Text(const uint8_t* data, size_t size, uint8_t encoding) {
    if (!data || size == 0) return {};
    switch (encoding) {
        case 0: {
            std::string raw(reinterpret_cast<const char*>(data), size);
            return CleanTagText(AnsiToWide(raw, 28591));
        }
        case 1: {
            if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
                size_t count = (size - 2) / 2;
                std::wstring out(count, L'\0');
                if (count) memcpy(out.data(), data + 2, count * 2);
                return CleanTagText(std::move(out));
            }
            if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
                size_t count = (size - 2) / 2;
                std::wstring out(count, L'\0');
                for (size_t i = 0; i < count; ++i) {
                    out[i] = static_cast<wchar_t>((static_cast<uint16_t>(data[2 + i * 2]) << 8) |
                                                  data[3 + i * 2]);
                }
                return CleanTagText(std::move(out));
            }
            size_t count = size / 2;
            std::wstring out(count, L'\0');
            if (count) memcpy(out.data(), data, count * 2);
            return CleanTagText(std::move(out));
        }
        case 2: {
            size_t count = size / 2;
            std::wstring out(count, L'\0');
            for (size_t i = 0; i < count; ++i) {
                out[i] = static_cast<wchar_t>((static_cast<uint16_t>(data[i * 2]) << 8) |
                                              data[i * 2 + 1]);
            }
            return CleanTagText(std::move(out));
        }
        case 3: {
            std::string raw(reinterpret_cast<const char*>(data), size);
            return CleanTagText(Utf8ToWide(raw));
        }
        default:
            return {};
    }
}

size_t StringTerminatorSize(uint8_t encoding) { return (encoding == 1 || encoding == 2) ? 2 : 1; }

bool ReadTerminatedString(const uint8_t* data, size_t size, size_t& pos, uint8_t encoding,
                          std::wstring& out) {
    size_t term = StringTerminatorSize(encoding);
    size_t start = pos;
    size_t i = pos;
    if (term == 2) {
        while (i + 1 < size) {
            if (data[i] == 0 && data[i + 1] == 0) break;
            i += 2;
        }
        if (i + 1 >= size) {
            pos = size;
            out = DecodeId3Text(data + start, size - start, encoding);
            return false;
        }
        out = DecodeId3Text(data + start, i - start, encoding);
        pos = i + 2;
    } else {
        while (i < size && data[i] != 0) ++i;
        out = DecodeId3Text(data + start, i - start, encoding);
        if (i < size) pos = i + 1;
        else pos = size;
    }
    return true;
}

bool ReadMimeTerminated(const uint8_t* data, size_t size, size_t& pos, std::string& out) {
    size_t start = pos;
    while (pos < size && data[pos] != 0) ++pos;
    out.assign(reinterpret_cast<const char*>(data + start), pos - start);
    if (pos < size) ++pos;
    return true;
}

std::vector<uint8_t> DeunsyncBytes(const uint8_t* data, size_t size) {
    std::vector<uint8_t> result;
    result.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        result.push_back(data[i]);
        if (data[i] == 0xFF && i + 1 < size && data[i + 1] == 0x00) ++i;
    }
    return result;
}

std::wstring MakeLrcFromSylt(const uint8_t* data, size_t size) {
    if (size < 7) return {};
    uint8_t encoding = data[0];
    size_t pos = 6;  // encoding + language + timestamp format + content type
    std::wstring descriptor;
    ReadTerminatedString(data, size, pos, encoding, descriptor);
    if (pos >= size) return {};
    struct Entry {
        int timeMs;
        std::wstring text;
    };
    std::vector<Entry> entries;
    while (pos < size) {
        std::wstring text;
        ReadTerminatedString(data, size, pos, encoding, text);
        if (pos + 4 > size) break;
        int timeMs = static_cast<int>(ReadU32BE(data + pos));
        pos += 4;
        if (!text.empty() || timeMs >= 0) entries.push_back({timeMs, text});
        if (pos >= size) break;
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.timeMs < b.timeMs; });
    std::wstring result;
    for (const auto& entry : entries) {
        result += LrcParser::FormatTimestamp(entry.timeMs);
        result += entry.text;
        result += L"\r\n";
    }
    return result;
}

int BitrateFromIndex(int version, int layer, int index) {
    static const int v1l1[15] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448};
    static const int v1l2[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
    static const int v1l3[15] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
    static const int v2l1[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
    static const int v2l23[15] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
    if (index <= 0 || index >= 15) return 0;
    if (version == 1) {
        if (layer == 1) return v1l1[index];
        if (layer == 2) return v1l2[index];
        return v1l3[index];
    }
    if (layer == 1) return v2l1[index];
    return v2l23[index];
}

int SampleRateFromIndex(int version, int index) {
    static const int rates[3] = {44100, 48000, 32000};
    if (index < 0 || index > 2) return 0;
    int base = rates[index];
    if (version == 2) return base / 2;
    if (version == 0) return base / 4;
    return base;
}

double EstimateMp3Duration(BinaryFile& file, uint64_t audioStart) {
    uint64_t fileSize = file.Size();
    uint64_t audioEnd = fileSize;
    if (fileSize >= 128) {
        uint8_t tag[3] = {};
        if (file.ReadAt(fileSize - 128, tag, 3) && tag[0] == 'T' && tag[1] == 'A' && tag[2] == 'G') {
            audioEnd -= 128;
        }
    }
    if (audioStart >= audioEnd) return 0.0;
    std::vector<uint8_t> head;
    size_t want = static_cast<size_t>(std::min<uint64_t>(audioEnd - audioStart, 512u * 1024u));
    if (!file.ReadAt(audioStart, head, want) || head.size() < 4) return 0.0;
    size_t frameAt = 0;
    bool found = false;
    for (size_t i = 0; i + 4 <= head.size(); ++i) {
        if (head[i] == 0xFF && (head[i + 1] & 0xE0) == 0xE0) {
            int versionBits = (head[i + 1] >> 3) & 3;
            int layerBits = (head[i + 1] >> 1) & 3;
            int bitrateIndex = (head[i + 2] >> 4) & 0x0F;
            int rateIndex = (head[i + 2] >> 2) & 3;
            if (versionBits != 1 && layerBits != 0 && bitrateIndex != 0 && bitrateIndex != 15 &&
                rateIndex != 3) {
                frameAt = i;
                found = true;
                break;
            }
        }
    }
    if (!found) return 0.0;
    int versionBits = (head[frameAt + 1] >> 3) & 3;
    int layerBits = (head[frameAt + 1] >> 1) & 3;
    int version = versionBits == 3 ? 1 : (versionBits == 2 ? 2 : 0);
    int layer = 4 - layerBits;  // 1, 2, 3
    int bitrateIndex = (head[frameAt + 2] >> 4) & 0x0F;
    int rateIndex = (head[frameAt + 2] >> 2) & 3;
    int sampleRate = SampleRateFromIndex(version, rateIndex);
    int bitrate = BitrateFromIndex(version, layer, bitrateIndex);
    int samplesPerFrame = layer == 1 ? 384 : (layer == 3 && version != 1 ? 576 : 1152);

    for (size_t i = frameAt; i + 12 < head.size() && i < frameAt + 220; ++i) {
        if (memcmp(head.data() + i, "Xing", 4) == 0 || memcmp(head.data() + i, "Info", 4) == 0) {
            uint32_t flags = ReadU32BE(head.data() + i + 4);
            size_t pos = i + 8;
            if (flags & 1) {
                if (pos + 4 <= head.size()) {
                    uint32_t frames = ReadU32BE(head.data() + pos);
                    if (sampleRate > 0 && frames > 0) {
                        return static_cast<double>(frames) * samplesPerFrame / sampleRate;
                    }
                }
            }
            break;
        }
    }
    if (bitrate > 0) {
        uint64_t bytes = audioEnd - audioStart;
        return static_cast<double>(bytes) * 8.0 / (bitrate * 1000.0);
    }
    return 0.0;
}

std::vector<uint8_t> Base64Decode(const std::string& input) {
    std::vector<uint8_t> out;
    int val = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        int decoded = -1;
        if (c >= 'A' && c <= 'Z') decoded = c - 'A';
        else if (c >= 'a' && c <= 'z') decoded = c - 'a' + 26;
        else if (c >= '0' && c <= '9') decoded = c - '0' + 52;
        else if (c == '+') decoded = 62;
        else if (c == '/') decoded = 63;
        else continue;
        val = (val << 6) | decoded;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

ArtworkBlob ParsePictureBlock(const std::vector<uint8_t>& data) {
    ArtworkBlob blob;
    if (data.size() < 32) return blob;
    size_t pos = 0;
    blob.pictureType = static_cast<int>(ReadU32BE(data.data() + pos));
    pos += 4;
    uint32_t mimeLen = ReadU32BE(data.data() + pos);
    pos += 4;
    if (pos + mimeLen > data.size()) return {};
    blob.mime.assign(reinterpret_cast<const char*>(data.data() + pos), mimeLen);
    pos += mimeLen;
    if (pos + 4 > data.size()) return {};
    uint32_t descLen = ReadU32BE(data.data() + pos);
    pos += 4;
    if (pos + descLen > data.size()) return {};
    pos += descLen;
    if (pos + 20 > data.size()) return {};
    pos += 16;  // width, height, depth, colors
    uint32_t imageLen = ReadU32BE(data.data() + pos);
    pos += 4;
    if (pos + imageLen > data.size() || imageLen == 0) return {};
    blob.bytes.assign(data.begin() + pos, data.begin() + pos + imageLen);
    if (blob.mime.empty()) {
        if (blob.bytes.size() > 8 && blob.bytes[0] == 0x89 && blob.bytes[1] == 'P') blob.mime = "image/png";
        else blob.mime = "image/jpeg";
    }
    return blob;
}

struct VorbisTags {
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring lyrics;
    ArtworkBlob artwork;
    std::string coverMime;
};

bool ParseVorbisComments(const uint8_t* data, size_t size, VorbisTags& tags) {
    size_t pos = 0;
    if (pos + 4 > size) return false;
    uint32_t vendorLen = ReadU32LE(data + pos);
    pos += 4;
    if (vendorLen > size - pos) return false;
    pos += vendorLen;
    if (pos + 4 > size) return false;
    uint32_t count = ReadU32LE(data + pos);
    pos += 4;
    std::vector<uint8_t> pendingPicture;
    std::string pendingMime;
    for (uint32_t i = 0; i < count && pos + 4 <= size; ++i) {
        uint32_t len = ReadU32LE(data + pos);
        pos += 4;
        if (len > size - pos) break;
        std::string entry(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        size_t eq = entry.find('=');
        if (eq == std::string::npos) continue;
        std::string key = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);
        for (char& c : key) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        std::wstring wide = CleanTagText(Utf8ToWide(value));
        if (key == "TITLE" && tags.title.empty()) tags.title = wide;
        else if (key == "ARTIST" && tags.artist.empty()) tags.artist = wide;
        else if (key == "ALBUM" && tags.album.empty()) tags.album = wide;
        else if ((key == "LYRICS" || key == "UNSYNCEDLYRICS" || key == "SYNCEDLYRICS") &&
                 tags.lyrics.empty()) {
            tags.lyrics = wide;
        } else if (key == "METADATA_BLOCK_PICTURE" || key == "COVERART") {
            pendingPicture = Base64Decode(value);
        } else if (key == "COVERARTMIME") {
            pendingMime = value;
        }
    }
    if (!pendingPicture.empty()) {
        if (pendingPicture.size() > 8 && pendingPicture[0] == 0x89 && pendingPicture[1] == 'P') {
            tags.artwork.bytes = std::move(pendingPicture);
            tags.artwork.mime = "image/png";
        } else if (pendingPicture.size() > 4 && pendingPicture[0] == 0x89) {
            tags.artwork.bytes = std::move(pendingPicture);
            tags.artwork.mime = "image/png";
        } else if (pendingPicture.size() > 4 && pendingPicture[0] == 0x89) {
            tags.artwork.bytes = std::move(pendingPicture);
            tags.artwork.mime = "image/png";
        } else {
            ArtworkBlob parsed = ParsePictureBlock(pendingPicture);
            if (parsed.Valid()) {
                tags.artwork = std::move(parsed);
            } else {
                tags.artwork.bytes = std::move(pendingPicture);
                tags.artwork.mime = pendingMime.empty() ? "image/jpeg" : pendingMime;
            }
        }
    }
    return true;
}

std::wstring SniffCodec(const std::wstring& extension) {
    if (extension == L".mp3") return L"MP3";
    if (extension == L".flac") return L"FLAC";
    if (extension == L".wav") return L"WAV";
    if (extension == L".m4a" || extension == L".m4b" || extension == L".mp4") return L"AAC / M4A";
    if (extension == L".aac") return L"AAC";
    if (extension == L".ogg" || extension == L".oga") return L"OGG";
    if (extension == L".opus") return L"OPUS";
    if (extension == L".wma") return L"WMA";
    if (extension == L".ape") return L"APE";
    if (extension == L".wv") return L"WavPack";
    if (extension == L".aiff" || extension == L".aif") return L"AIFF";
    return L"AUDIO";
}

}  // namespace

TrackInfo TagReader::Read(const std::wstring& path, bool withArtwork, bool withLyrics) const {
    TrackInfo info;
    info.codec = SniffCodec(ExtensionOf(path));
    BinaryFile file;
    if (!file.Open(path)) return info;

    uint8_t magic[12] = {};
    size_t magicSize = static_cast<size_t>(std::min<uint64_t>(12, file.Size()));
    if (magicSize && !file.ReadAt(0, magic, magicSize)) return info;

    bool parsedSpecific = false;
    if (magicSize >= 4 && memcmp(magic, "fLaC", 4) == 0) {
        ReadFlac(file, info, withArtwork, withLyrics);
        parsedSpecific = true;
    } else if (magicSize >= 4 && memcmp(magic, "OggS", 4) == 0) {
        ReadOgg(file, info, withArtwork, withLyrics);
        parsedSpecific = true;
    } else if (magicSize >= 4 && memcmp(magic, "RIFF", 4) == 0) {
        ReadWav(file, info);
        parsedSpecific = true;
    } else if (magicSize >= 8 && memcmp(magic + 4, "ftyp", 4) == 0) {
        ReadMp4(file, info, withArtwork, withLyrics);
        parsedSpecific = true;
    }

    uint64_t audioStart = 0;
    if (magicSize >= 3 && magic[0] == 'I' && magic[1] == 'D' && magic[2] == '3') {
        if (magicSize >= 10) {
            uint32_t tagSize = SyncSafe32(magic + 6);
            audioStart = 10ull + tagSize;
            if (magic[5] & 0x10) audioStart += 10;  // footer
            if (audioStart > file.Size()) audioStart = file.Size();
        }
        ReadId3v2(file, audioStart, info, withArtwork, withLyrics);
    }

    if (!parsedSpecific && (ExtensionOf(path) == L".mp3" ||
                            (magicSize >= 2 && magic[0] == 0xFF))) {
        info.codec = L"MP3";
        if (info.durationSec <= 0.0) info.durationSec = EstimateMp3Duration(file, audioStart);
    }

    if (!parsedSpecific && ExtensionOf(path) == L".wma") {
        info.codec = L"WMA";
    }

    ReadId3v1(file, info);
    std::wstring ext = ExtensionOf(path);
    if ((withArtwork && !info.artwork.Valid()) || ext == L".ape" || ext == L".wv") {
        ReadApeTag(file, info, withArtwork);
    }
    if (info.title.empty()) info.title = StemOf(path);
    info.title = CleanTagText(info.title);
    info.artist = CleanTagText(info.artist);
    info.album = CleanTagText(info.album);
    if (!info.artwork.Valid()) info.hasArtwork = false;
    else info.hasArtwork = true;
    return info;
}

void TagReader::ReadId3v2(BinaryFile& file, uint64_t tagEnd, TrackInfo& info, bool withArtwork,
                          bool withLyrics) const {
    (void)tagEnd;
    if (file.Size() < 10) return;
    uint8_t header[10] = {};
    if (!file.ReadAt(0, header, 10)) return;
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') return;
    uint8_t major = header[3];
    uint8_t flags = header[5];
    uint32_t tagSize = SyncSafe32(header + 6);
    if (tagSize == 0) return;
    uint64_t payloadSize = std::min<uint64_t>(tagSize, file.Size() - 10);
    payloadSize = std::min<uint64_t>(payloadSize, kMaxTagRead);
    std::vector<uint8_t> tag;
    if (!file.ReadAt(10, tag, static_cast<size_t>(payloadSize))) return;
    if (flags & 0x80) tag = DeunsyncBytes(tag.data(), tag.size());
    if (tag.size() < 4) return;

    size_t pos = 0;
    if (flags & 0x40) {
        if (major == 3 && tag.size() >= 4) {
            uint32_t ext = ReadU32BE(tag.data());
            pos = static_cast<size_t>(std::min<uint64_t>(4ull + ext, tag.size()));
        } else if (major == 4 && tag.size() >= 4) {
            uint32_t ext = SyncSafe32(tag.data());
            pos = static_cast<size_t>(std::min<uint64_t>(ext, tag.size()));
        }
    }

    ArtworkBlob bestArt;
    bool haveBest = false;
    std::wstring syltLyrics;
    std::wstring usltLyrics;

    while (pos < tag.size()) {
        std::string frameId;
        uint32_t frameSize = 0;
        uint16_t frameFlags = 0;
        size_t frameHeader = 0;
        if (major == 2) {
            if (pos + 6 > tag.size()) break;
            if (tag[pos] == 0) break;
            frameId.assign(reinterpret_cast<const char*>(&tag[pos]), 3);
            frameSize = ReadU24BE(&tag[pos + 3]);
            frameHeader = 6;
        } else {
            if (pos + 10 > tag.size()) break;
            if (tag[pos] == 0) break;
            frameId.assign(reinterpret_cast<const char*>(&tag[pos]), 4);
            frameSize = major == 4 ? SyncSafe32(&tag[pos + 4]) : ReadU32BE(&tag[pos + 4]);
            frameFlags = static_cast<uint16_t>((tag[pos + 8] << 8) | tag[pos + 9]);
            frameHeader = 10;
        }
        if (frameSize == 0 || pos + frameHeader + frameSize > tag.size()) break;
        const uint8_t* data = tag.data() + pos + frameHeader;
        size_t size = frameSize;

        bool usable = true;
        if (major == 3) {
            if (frameFlags & 0x00C0) usable = false;  // compressed / encrypted
            if (frameFlags & 0x0020) {                 // grouped
                if (size < 1) usable = false;
                else { data += 1; size -= 1; }
            }
        } else if (major == 4) {
            uint8_t formatFlags = static_cast<uint8_t>(frameFlags & 0xFF);
            if (formatFlags & 0x0C) usable = false;
            if (formatFlags & 0x40) {
                if (size < 1) usable = false;
                else { data += 1; size -= 1; }
            }
            if (formatFlags & 0x01) {
                if (size < 4) usable = false;
                else { data += 4; size -= 4; }
            }
        }

        std::vector<uint8_t> frameStore;
        if (major == 4 && usable && (frameFlags & 0x0002)) {
            frameStore = DeunsyncBytes(data, size);
            data = frameStore.data();
            size = frameStore.size();
        }

        auto textFrame = [&](std::wstring& target) {
            if (!usable || size < 1) return;
            if (target.empty()) {
                target = DecodeId3Text(data + 1, size - 1, data[0]);
            }
        };

        if (major == 2) {
            if (frameId == "TT2") textFrame(info.title);
            else if (frameId == "TP1") textFrame(info.artist);
            else if (frameId == "TAL") textFrame(info.album);
            else if (frameId == "TYE") textFrame(info.year);
            else if (frameId == "TCO") textFrame(info.genre);
            else if (frameId == "TLE") {
                std::wstring len;
                textFrame(len);
                if (!len.empty()) {
                    int ms = static_cast<int>(std::wcstol(len.c_str(), nullptr, 10));
                    if (ms > 0) info.durationSec = ms / 1000.0;
                }
            }
        } else {
            if (frameId == "TIT2") textFrame(info.title);
            else if (frameId == "TPE1") textFrame(info.artist);
            else if (frameId == "TALB") textFrame(info.album);
            else if (frameId == "TDRC" || frameId == "TYER") textFrame(info.year);
            else if (frameId == "TCON") textFrame(info.genre);
            else if (frameId == "TLEN") {
                std::wstring len;
                textFrame(len);
                if (!len.empty()) {
                    int ms = static_cast<int>(std::wcstol(len.c_str(), nullptr, 10));
                    if (ms > 0) info.durationSec = ms / 1000.0;
                }
            }
        }

        if (usable && withLyrics) {
            if (frameId == "USLT" || frameId == "ULT") {
                if (size > 4) {
                    uint8_t enc = data[0];
                    size_t p = 4;
                    std::wstring desc;
                    ReadTerminatedString(data, size, p, enc, desc);
                    std::wstring text = DecodeId3Text(data + p, size - p, enc);
                    if (!text.empty() && usltLyrics.empty()) usltLyrics = text;
                }
            } else if (frameId == "SYLT" || frameId == "SLT") {
                if (size > 7 && data[4] == 1) {
                    std::wstring lrc = MakeLrcFromSylt(data, size);
                    if (!lrc.empty()) syltLyrics = std::move(lrc);
                }
            }
        }

        if (usable && withArtwork) {
            ArtworkBlob art;
            if ((frameId == "APIC" || frameId == "PIC") && size > 4) {
                uint8_t enc = data[0];
                size_t p = 1;
                std::string mime;
                if (frameId == "PIC") {
                    if (p + 3 <= size) {
                        std::string fmt(reinterpret_cast<const char*>(data + p), 3);
                        p += 3;
                        for (char& c : fmt) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                        mime = fmt == "png" ? "image/png" : "image/jpeg";
                    }
                } else {
                    ReadMimeTerminated(data, size, p, mime);
                }
                if (p < size) {
                    art.pictureType = data[p++];
                    std::wstring desc;
                    ReadTerminatedString(data, size, p, enc, desc);
                    if (p < size) art.bytes.assign(data + p, data + size);
                }
                if (art.mime.empty()) art.mime = mime;
                if (art.mime.empty()) {
                    if (art.bytes.size() > 3 && art.bytes[0] == 0xFF && art.bytes[1] == 0xD8)
                        art.mime = "image/jpeg";
                    else if (art.bytes.size() > 4 && art.bytes[0] == 0x89)
                        art.mime = "image/png";
                    else art.mime = "image/jpeg";
                }
                if (art.Valid() && (!haveBest || art.pictureType == 3)) {
                    bestArt = std::move(art);
                    haveBest = true;
                    if (bestArt.pictureType == 3) {
                        // Front cover is the preferred image; keep parsing only cheap frames.
                    }
                }
            }
        }
        pos += frameHeader + frameSize;
    }

    if (!syltLyrics.empty()) {
        info.embeddedLyrics = std::move(syltLyrics);
        info.embeddedLyricsTimed = true;
    } else if (!usltLyrics.empty()) {
        info.embeddedLyrics = std::move(usltLyrics);
        LrcParseResult parsed = LrcParser::Parse(info.embeddedLyrics);
        info.embeddedLyricsTimed = parsed.timed;
    }
    if (haveBest) info.artwork = std::move(bestArt);
}

void TagReader::ReadId3v1(BinaryFile& file, TrackInfo& info) const {
    if (file.Size() < 128) return;
    uint8_t tag[128] = {};
    if (!file.ReadAt(file.Size() - 128, tag, 128)) return;
    if (memcmp(tag, "TAG", 3) != 0) return;
    auto field = [](const uint8_t* data, size_t len) {
        size_t actual = len;
        while (actual > 0 && (data[actual - 1] == 0 || data[actual - 1] == ' ')) --actual;
        std::string raw(reinterpret_cast<const char*>(data), actual);
        return CleanTagText(AnsiToWide(raw, CP_ACP));
    };
    std::wstring title = field(tag + 3, 30);
    std::wstring artist = field(tag + 33, 30);
    std::wstring album = field(tag + 63, 30);
    if (info.title.empty() && !title.empty()) info.title = title;
    if (info.artist.empty() && !artist.empty()) info.artist = artist;
    if (info.album.empty() && !album.empty()) info.album = album;
}

void TagReader::ReadFlac(BinaryFile& file, TrackInfo& info, bool withArtwork,
                         bool withLyrics) const {
    info.codec = L"FLAC";
    uint64_t pos = 4;
    while (pos + 4 <= file.Size()) {
        uint8_t header[4] = {};
        if (!file.ReadAt(pos, header, 4)) break;
        bool last = (header[0] & 0x80) != 0;
        int type = header[0] & 0x7F;
        uint32_t len = ReadU24BE(header + 1);
        uint64_t dataPos = pos + 4;
        if (dataPos + len > file.Size()) break;

        if (type == 0 && len >= 34) {
            uint8_t streamInfo[34] = {};
            if (file.ReadAt(dataPos, streamInfo, 34)) {
                uint32_t sampleRate = (static_cast<uint32_t>(streamInfo[10]) << 12) |
                                      (static_cast<uint32_t>(streamInfo[11]) << 4) |
                                      (streamInfo[12] >> 4);
                uint64_t totalSamples = (static_cast<uint64_t>(streamInfo[13] & 0x0F) << 32) |
                                        (static_cast<uint64_t>(streamInfo[14]) << 24) |
                                        (static_cast<uint64_t>(streamInfo[15]) << 16) |
                                        (static_cast<uint64_t>(streamInfo[16]) << 8) |
                                        streamInfo[17];
                if (sampleRate > 0 && totalSamples > 0) {
                    info.durationSec = static_cast<double>(totalSamples) / sampleRate;
                }
            }
        } else if (type == 4 && len > 0) {
            if (len <= 32u * 1024u * 1024u) {
                std::vector<uint8_t> block;
                if (file.ReadAt(dataPos, block, len)) {
                    VorbisTags tags;
                    if (ParseVorbisComments(block.data(), block.size(), tags)) {
                        if (info.title.empty() && !tags.title.empty()) info.title = tags.title;
                        if (info.artist.empty()) info.artist = tags.artist;
                        if (info.album.empty()) info.album = tags.album;
                        if (withLyrics && info.embeddedLyrics.empty()) {
                            info.embeddedLyrics = tags.lyrics;
                            info.embeddedLyricsTimed =
                                LrcParser::Parse(tags.lyrics).timed;
                        }
                        if (withArtwork && tags.artwork.Valid()) info.artwork = std::move(tags.artwork);
                    }
                }
            }
        } else if (type == 6 && withArtwork && len > 32) {
            if (len <= 32u * 1024u * 1024u) {
                std::vector<uint8_t> block;
                if (file.ReadAt(dataPos, block, len)) {
                    ArtworkBlob art = ParsePictureBlock(block);
                    if (art.Valid()) {
                        if (!info.artwork.Valid() || art.pictureType == 3) {
                            info.artwork = std::move(art);
                        }
                    }
                }
            }
        }
        pos = dataPos + len;
        if (last) break;
    }
}

void TagReader::ReadOgg(BinaryFile& file, TrackInfo& info, bool withArtwork,
                        bool withLyrics) const {
    std::vector<uint8_t> head;
    if (!file.ReadAtMost(0, head, 2u * 1024u * 1024u) || head.size() < 27) return;
    bool isOpus = false;
    bool identified = false;
    uint32_t sampleRate = 0;
    std::vector<uint8_t> packet(1024 * 1024);
    size_t packetLen = 0;
    int packetIndex = 0;
    size_t pos = 0;
    while (pos + 27 <= head.size() && packetIndex < 6) {
        if (memcmp(head.data() + pos, "OggS", 4) != 0) break;
        uint8_t segments = head[pos + 26];
        if (pos + 27 + segments > head.size()) break;
        size_t dataPos = pos + 27 + segments;
        size_t dataLen = 0;
        for (uint8_t i = 0; i < segments; ++i) dataLen += head[pos + 27 + i];
        if (dataPos + dataLen > head.size()) break;
        size_t offset = dataPos;
        for (uint8_t i = 0; i < segments; ++i) {
            size_t segLen = head[pos + 27 + i];
            if (packetLen + segLen <= sizeof(packet)) {
                memcpy(packet.data() + packetLen, head.data() + offset, segLen);
                packetLen += segLen;
            }
            offset += segLen;
            if (segLen < 255) {
                if (packetIndex == 0) {
                    if (packetLen >= 16 && memcmp(packet.data(), "\x01vorbis", 7) == 0) {
                        sampleRate = ReadU32LE(packet.data() + 12);
                        identified = true;
                    } else if (packetLen >= 12 && memcmp(packet.data(), "OpusHead", 8) == 0) {
                        sampleRate = 48000;
                        isOpus = true;
                        identified = true;
                    }
                } else if (packetLen > 0) {
                    const uint8_t* comments = nullptr;
                    size_t commentsLen = 0;
                    if (!isOpus && packetLen >= 10 && memcmp(packet.data(), "\x03vorbis", 7) == 0) {
                        comments = packet.data() + 7;
                        commentsLen = packetLen - 7;
                    } else if (isOpus && packetLen >= 12 && memcmp(packet.data(), "OpusTags", 8) == 0) {
                        comments = packet.data() + 8;
                        commentsLen = packetLen - 8;
                    }
                    if (comments && commentsLen >= 8) {
                        VorbisTags tags;
                        if (ParseVorbisComments(comments, commentsLen, tags)) {
                            if (info.title.empty() && !tags.title.empty()) info.title = tags.title;
                            if (info.artist.empty()) info.artist = tags.artist;
                            if (info.album.empty()) info.album = tags.album;
                            if (withLyrics && info.embeddedLyrics.empty()) {
                                info.embeddedLyrics = tags.lyrics;
                                info.embeddedLyricsTimed =
                                    LrcParser::Parse(tags.lyrics).timed;
                            }
                            if (withArtwork && tags.artwork.Valid())
                                info.artwork = std::move(tags.artwork);
                        }
                    }
                }
                ++packetIndex;
                packetLen = 0;
            }
        }
        pos = dataPos + dataLen;
    }
    info.codec = isOpus ? L"OPUS" : L"OGG VORBIS";
    if (!identified || sampleRate == 0) return;

    size_t tailSize = static_cast<size_t>(std::min<uint64_t>(file.Size(), 256u * 1024u));
    std::vector<uint8_t> tail;
    if (!file.ReadAt(file.Size() - tailSize, tail, tailSize)) return;
    for (size_t i = tail.size(); i >= 27; --i) {
        size_t at = i - 27;
        if (memcmp(tail.data() + at, "OggS", 4) != 0) continue;
        uint64_t granule = ReadU64LE(tail.data() + at + 6);
        if (granule > 0) {
            info.durationSec = static_cast<double>(granule) / sampleRate;
            break;
        }
    }
}

namespace {

struct Mp4Atom {
    uint32_t type = 0;
    uint64_t begin = 0;
    uint64_t dataBegin = 0;
    uint64_t end = 0;
};

std::string Mp4TagText(const std::vector<uint8_t>& data, uint32_t dataType) {
    if (data.size() <= 8) return {};
    const uint8_t* payload = data.data() + 8;
    size_t len = data.size() - 8;
    if (dataType == 2) {
        std::wstring wide;
        for (size_t i = 0; i + 1 < len; i += 2) {
            wide.push_back(static_cast<wchar_t>((static_cast<uint16_t>(payload[i]) << 8) |
                                                payload[i + 1]));
        }
        return WideToUtf8(wide);
    }
    return std::string(reinterpret_cast<const char*>(payload), len);
}

uint32_t ReadFourCC(const uint8_t* p) { return ReadU32BE(p); }

void WalkMp4(BinaryFile& file, uint64_t begin, uint64_t end, uint32_t parentType, int depth,
             TrackInfo& info, bool withArtwork, bool withLyrics, double& mvhdScale,
             uint64_t& mvhdDuration) {
    if (depth > 8 || begin >= end) return;
    uint64_t pos = begin;
    while (pos + 8 <= end) {
        uint8_t hdr[16] = {};
        size_t headerSize = 8;
        if (!file.ReadAt(pos, hdr, 8)) break;
        uint64_t size = ReadU32BE(hdr);
        uint32_t type = ReadFourCC(hdr + 4);
        if (size == 1) {
            if (!file.ReadAt(pos + 8, hdr + 8, 8)) break;
            size = ReadU64BE(hdr + 8);
            headerSize = 16;
        } else if (size == 0) {
            size = end - pos;
        }
        if (size < headerSize || pos + size > end) break;
        uint64_t dataBegin = pos + headerSize;
        uint64_t atomEnd = pos + size;
        std::string name(reinterpret_cast<const char*>(hdr + 4), 4);

        if (type == 0x6D766864 /* mvhd */ && atomEnd - dataBegin >= 20) {
            std::vector<uint8_t> data;
            if (file.ReadAt(dataBegin, data, static_cast<size_t>(std::min<uint64_t>(64, atomEnd - dataBegin)))) {
                uint8_t version = data[0];
                if (version == 0 && data.size() >= 20) {
                    mvhdScale = ReadU32BE(data.data() + 12);
                    mvhdDuration = ReadU32BE(data.data() + 16);
                } else if (version == 1 && data.size() >= 32) {
                    mvhdScale = ReadU32BE(data.data() + 20);
                    mvhdDuration = ReadU64BE(data.data() + 24);
                }
            }
        } else if (type == 0x6D657461 /* meta */) {
            uint64_t childBegin = dataBegin;
            uint8_t peek[4] = {};
            if (childBegin + 4 <= atomEnd && file.ReadAt(childBegin, peek, 4) &&
                peek[0] == 0 && peek[1] == 0 && peek[2] == 0 && peek[3] == 0) {
                childBegin += 4;
            }
            WalkMp4(file, childBegin, atomEnd, type, depth + 1, info, withArtwork, withLyrics,
                    mvhdScale, mvhdDuration);
        } else if (type == 0x6D6F6F76 /* moov */ || type == 0x75647461 /* udta */ ||
                   type == 0x696C7374 /* ilst */ || (parentType == 0x696C7374)) {
            WalkMp4(file, dataBegin, atomEnd, type, depth + 1, info, withArtwork, withLyrics,
                    mvhdScale, mvhdDuration);
        } else if (type == 0x64617461 /* data */ && parentType != 0) {
            std::vector<uint8_t> data;
            size_t readLen = static_cast<size_t>(std::min<uint64_t>(8u * 1024u * 1024u, atomEnd - dataBegin));
            if (file.ReadAt(dataBegin, data, readLen) && data.size() >= 8) {
                uint32_t dataType = ReadU32BE(data.data());
                bool isCover = parentType == 0x636F7672;  // covr
                auto setText = [&](std::wstring& target) {
                    if (!target.empty()) return;
                    std::string utf8 = Mp4TagText(data, dataType);
                    target = CleanTagText(Utf8ToWide(utf8));
                };
                if (parentType == 0xA96E616D) setText(info.title);        // 漏nam
                else if (parentType == 0xA9415254) setText(info.artist);  // 漏ART
                else if (parentType == 0xA9616C62) setText(info.album);   // 漏alb
                else if (parentType == 0xA9646179) setText(info.year);    // 漏day
                else if (parentType == 0xA967656E) setText(info.genre);   // 漏gen
                else if (parentType == 0xA96C7972 && withLyrics) setText(info.embeddedLyrics);  // 漏lyr
                else if (isCover && withArtwork && data.size() > 8) {
                    ArtworkBlob art;
                    art.bytes.assign(data.begin() + 8, data.end());
                    art.mime = dataType == 14 ? "image/png" : "image/jpeg";
                    if (art.Valid()) info.artwork = std::move(art);
                }
            }
        }
        pos = atomEnd;
    }
}

}  // namespace

void TagReader::ReadMp4(BinaryFile& file, TrackInfo& info, bool withArtwork,
                        bool withLyrics) const {
    info.codec = L"AAC / M4A";
    double scale = 0;
    uint64_t duration = 0;
    WalkMp4(file, 0, file.Size(), 0, 0, info, withArtwork, withLyrics, scale, duration);
    if (scale > 0 && duration > 0) info.durationSec = static_cast<double>(duration) / scale;
    if (!info.embeddedLyrics.empty()) {
        info.embeddedLyricsTimed = LrcParser::Parse(info.embeddedLyrics).timed;
    }
}

void TagReader::ReadWav(BinaryFile& file, TrackInfo& info) const {
    info.codec = L"WAV";
    uint64_t pos = 12;
    uint32_t byteRate = 0;
    uint64_t dataSize = 0;
    while (pos + 8 <= file.Size()) {
        uint8_t header[8] = {};
        if (!file.ReadAt(pos, header, 8)) break;
        uint32_t chunkSize = ReadU32LE(header + 4);
        uint64_t dataPos = pos + 8;
        if (memcmp(header, "fmt ", 4) == 0 && chunkSize >= 16) {
            uint8_t fmt[16] = {};
            if (file.ReadAt(dataPos, fmt, 16)) byteRate = ReadU32LE(fmt + 8);
        } else if (memcmp(header, "data", 4) == 0) {
            dataSize = chunkSize;
            break;
        } else if (memcmp(header, "LIST", 4) == 0 && chunkSize > 4) {
            std::vector<uint8_t> list;
            size_t amount = static_cast<size_t>(std::min<uint64_t>(chunkSize, 1u * 1024u * 1024u));
            if (file.ReadAt(dataPos, list, amount) && list.size() >= 12) {
                size_t p = 12;
                while (p + 8 <= list.size()) {
                    uint32_t len = ReadU32LE(list.data() + p + 4);
                    if (p + 8 + len > list.size()) break;
                    std::string key(reinterpret_cast<const char*>(list.data() + p), 4);
                    std::string value(reinterpret_cast<const char*>(list.data() + p + 8), len);
                    while (!value.empty() && value.back() == 0) value.pop_back();
                    if (key == "INAM" && info.title.empty()) info.title = Utf8ToWide(value);
                    else if (key == "IART" && info.artist.empty()) info.artist = Utf8ToWide(value);
                    else if (key == "IPRD" && info.album.empty()) info.album = Utf8ToWide(value);
                    p += 8 + len + (len & 1);
                }
            }
        }
        pos = dataPos + chunkSize + (chunkSize & 1);
    }
    if (byteRate > 0 && dataSize > 0) {
        info.durationSec = static_cast<double>(dataSize) / byteRate;
    }
}

void TagReader::ReadApeTag(BinaryFile& file, TrackInfo& info, bool withArtwork) const {
    if (file.Size() < 64) return;
    size_t tailSize = static_cast<size_t>(std::min<uint64_t>(file.Size(), 1024u * 1024u));
    std::vector<uint8_t> tail;
    if (!file.ReadAt(file.Size() - tailSize, tail, tailSize)) return;
    size_t footer = std::string::npos;
    for (size_t i = tail.size(); i >= 32; --i) {
        size_t at = i - 32;
        if (memcmp(tail.data() + at, "APETAGEX", 8) == 0) {
            footer = at;
            break;
        }
    }
    if (footer == std::string::npos) return;
    uint32_t tagSize = ReadU32LE(tail.data() + footer + 12);
    uint32_t itemCount = ReadU32LE(tail.data() + footer + 16);
    if (tagSize < 32 || tagSize > tail.size()) return;
    size_t start = tail.size() - tagSize;
    size_t pos = start;
    size_t end = footer;
    for (uint32_t i = 0; i < itemCount && pos + 8 <= end; ++i) {
        uint32_t valueSize = ReadU32LE(tail.data() + pos);
        uint32_t flags = ReadU32LE(tail.data() + pos + 4);
        pos += 8;
        size_t keyEnd = pos;
        while (keyEnd < end && tail[keyEnd] != 0) ++keyEnd;
        if (keyEnd >= end) break;
        std::string key(reinterpret_cast<const char*>(tail.data() + pos), keyEnd - pos);
        pos = keyEnd + 1;
        if (valueSize > end - pos) break;
        const uint8_t* value = tail.data() + pos;
        size_t valueLen = valueSize;
        if (key == "Title" && info.title.empty()) info.title = Utf8ToWide(std::string(reinterpret_cast<const char*>(value), valueLen));
        else if (key == "Artist" && info.artist.empty()) info.artist = Utf8ToWide(std::string(reinterpret_cast<const char*>(value), valueLen));
        else if (key == "Album" && info.album.empty()) info.album = Utf8ToWide(std::string(reinterpret_cast<const char*>(value), valueLen));
        else if (withArtwork && StartsWith(Utf8ToWide(key), L"Cover Art") && valueLen > 4) {
            size_t nameEnd = 0;
            while (nameEnd < valueLen && value[nameEnd] != 0) ++nameEnd;
            if (nameEnd + 1 < valueLen) {
                ArtworkBlob art;
                art.bytes.assign(value + nameEnd + 1, value + valueLen);
                art.mime = (art.bytes.size() > 4 && art.bytes[0] == 0x89) ? "image/png" : "image/jpeg";
                info.artwork = std::move(art);
            }
        }
        pos += valueSize;
        (void)flags;
    }
}

}  // namespace mk


