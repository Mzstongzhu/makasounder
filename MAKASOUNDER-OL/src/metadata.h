#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mk {

struct ArtworkBlob {
    std::vector<uint8_t> bytes;
    std::string mime;
    int pictureType = 0;
    bool Valid() const { return !bytes.empty(); }
};

struct TrackInfo {
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring year;
    std::wstring genre;
    std::wstring codec;
    double durationSec = 0.0;
    int bitrateKbps = 0;
    bool hasArtwork = false;
    ArtworkBlob artwork;
    std::wstring embeddedLyrics;
    bool embeddedLyricsTimed = false;
};

class TagReader {
public:
    TrackInfo Read(const std::wstring& path, bool withArtwork, bool withLyrics) const;

private:
    void ReadId3v2(class BinaryFile& file, uint64_t tagEnd, TrackInfo& info, bool withArtwork,
                   bool withLyrics) const;
    void ReadId3v1(class BinaryFile& file, TrackInfo& info) const;
    void ReadFlac(class BinaryFile& file, TrackInfo& info, bool withArtwork, bool withLyrics) const;
    void ReadOgg(class BinaryFile& file, TrackInfo& info, bool withArtwork, bool withLyrics) const;
    void ReadMp4(class BinaryFile& file, TrackInfo& info, bool withArtwork, bool withLyrics) const;
    void ReadWav(class BinaryFile& file, TrackInfo& info) const;
    void ReadApeTag(class BinaryFile& file, TrackInfo& info, bool withArtwork) const;
};

}  // namespace mk
