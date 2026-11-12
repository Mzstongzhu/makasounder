#pragma once

#include <windows.h>

#include <gdiplus.h>

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "metadata.h"

namespace mk {

class ArtworkService {
public:
    void Initialize(HINSTANCE instance);
    void Shutdown();

    std::shared_ptr<Gdiplus::Bitmap> LoadFor(const std::wstring& key, const ArtworkBlob& blob);
    std::shared_ptr<Gdiplus::Bitmap> Placeholder();
    void Clear();

private:
    std::shared_ptr<Gdiplus::Bitmap> Decode(const std::vector<uint8_t>& bytes);
    std::shared_ptr<Gdiplus::Bitmap> MakePlaceholderFromResource();
    std::shared_ptr<Gdiplus::Bitmap> MakePlaceholderFallback();
    void Remember(const std::wstring& key, const std::shared_ptr<Gdiplus::Bitmap>& bitmap);

    HINSTANCE instance_ = nullptr;
    ULONG_PTR token_ = 0;
    std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> cache_;
    std::deque<std::wstring> order_;
    std::shared_ptr<Gdiplus::Bitmap> placeholder_;
};

}  // namespace mk
