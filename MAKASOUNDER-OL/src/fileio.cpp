#include "fileio.h"

#include <algorithm>

namespace mk {

BinaryFile::~BinaryFile() { Close(); }

bool BinaryFile::Open(const std::wstring& path) {
    Close();
    handle_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                          nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle_, &size) || size.QuadPart < 0) {
        Close();
        return false;
    }
    size_ = static_cast<uint64_t>(size.QuadPart);
    return true;
}

void BinaryFile::Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    size_ = 0;
}

bool BinaryFile::ReadAt(uint64_t offset, void* data, size_t size) const {
    if (handle_ == INVALID_HANDLE_VALUE || offset > size_ || size > size_ - offset) return false;
    auto* out = static_cast<uint8_t*>(data);
    size_t done = 0;
    while (done < size) {
        LARGE_INTEGER seekPos{};
        seekPos.QuadPart = static_cast<LONGLONG>(offset + done);
        if (!SetFilePointerEx(handle_, seekPos, nullptr, FILE_BEGIN)) return false;
        DWORD read = 0;
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(size - done, 1u << 20));
        if (!ReadFile(handle_, out + done, chunk, &read, nullptr) || read == 0) return false;
        done += read;
    }
    return true;
}

bool BinaryFile::ReadAt(uint64_t offset, std::vector<uint8_t>& out, size_t size) const {
    out.resize(size);
    if (size == 0) return true;
    return ReadAt(offset, out.data(), size);
}

bool BinaryFile::ReadAtMost(uint64_t offset, std::vector<uint8_t>& out, size_t maxSize) const {
    if (offset >= size_) {
        out.clear();
        return true;
    }
    size_t amount = static_cast<size_t>(std::min<uint64_t>(maxSize, size_ - offset));
    return ReadAt(offset, out, amount);
}

uint16_t ReadU16BE(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t ReadU24BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 16) | (static_cast<uint32_t>(p[1]) << 8) | p[2];
}

uint32_t ReadU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

uint64_t ReadU64BE(const uint8_t* p) {
    return (static_cast<uint64_t>(ReadU32BE(p)) << 32) | ReadU32BE(p + 4);
}

uint32_t ReadU32LE(const uint8_t* p) {
    return p[0] | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t ReadU64LE(const uint8_t* p) {
    return ReadU32LE(p) | (static_cast<uint64_t>(ReadU32LE(p + 4)) << 32);
}

uint32_t SyncSafe32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0] & 0x7F) << 21) | (static_cast<uint32_t>(p[1] & 0x7F) << 14) |
           (static_cast<uint32_t>(p[2] & 0x7F) << 7) | static_cast<uint32_t>(p[3] & 0x7F);
}

}  // namespace mk
