#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace mk {

class BinaryFile {
public:
    BinaryFile() = default;
    ~BinaryFile();
    BinaryFile(const BinaryFile&) = delete;
    BinaryFile& operator=(const BinaryFile&) = delete;

    bool Open(const std::wstring& path);
    void Close();
    bool IsOpen() const { return handle_ != INVALID_HANDLE_VALUE; }
    uint64_t Size() const { return size_; }
    bool ReadAt(uint64_t offset, void* data, size_t size) const;
    bool ReadAt(uint64_t offset, std::vector<uint8_t>& out, size_t size) const;
    bool ReadAtMost(uint64_t offset, std::vector<uint8_t>& out, size_t maxSize) const;

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    uint64_t size_ = 0;
};

uint16_t ReadU16BE(const uint8_t* p);
uint32_t ReadU24BE(const uint8_t* p);
uint32_t ReadU32BE(const uint8_t* p);
uint64_t ReadU64BE(const uint8_t* p);
uint32_t ReadU32LE(const uint8_t* p);
uint64_t ReadU64LE(const uint8_t* p);
uint32_t SyncSafe32(const uint8_t* p);

}  // namespace mk
