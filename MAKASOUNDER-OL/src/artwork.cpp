#include "artwork.h"

#include "common.h"

#include <shlwapi.h>

#include <algorithm>
#include <cstring>

namespace mk {

namespace {

constexpr int kArtSize = 512;
constexpr size_t kCacheLimit = 8;
constexpr int kPlaceholderResource = 101;

struct StreamReleaser {
    void operator()(IStream* stream) const {
        if (stream) stream->Release();
    }
};

}  // namespace

void ArtworkService::Initialize(HINSTANCE instance) {
    instance_ = instance;
    cache_.clear();
    order_.clear();
    placeholder_.reset();
}

void ArtworkService::Shutdown() {
    cache_.clear();
    order_.clear();
    placeholder_.reset();
    instance_ = nullptr;
}

std::shared_ptr<Gdiplus::Bitmap> ArtworkService::Decode(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return {};
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!memory) return {};
    void* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        return {};
    }
    memcpy(destination, bytes.data(), bytes.size());
    GlobalUnlock(memory);
    IStream* raw = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &raw)) || !raw) {
        GlobalFree(memory);
        return {};
    }
    std::unique_ptr<IStream, StreamReleaser> stream(raw);
    auto source = std::make_shared<Gdiplus::Bitmap>(raw, FALSE);
    if (source->GetLastStatus() != Gdiplus::Ok || source->GetWidth() == 0 ||
        source->GetHeight() == 0) {
        return {};
    }

    auto canvas = std::make_shared<Gdiplus::Bitmap>(kArtSize, kArtSize,
                                                    PixelFormat32bppPARGB);
    if (canvas->GetLastStatus() != Gdiplus::Ok) return {};
    Gdiplus::Graphics graphics(canvas.get());
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    float srcW = static_cast<float>(source->GetWidth());
    float srcH = static_cast<float>(source->GetHeight());
    float scale = std::max(static_cast<float>(kArtSize) / srcW,
                           static_cast<float>(kArtSize) / srcH);
    float drawW = srcW * scale;
    float drawH = srcH * scale;
    float x = (kArtSize - drawW) * 0.5f;
    float y = (kArtSize - drawH) * 0.5f;
    graphics.DrawImage(source.get(), Gdiplus::RectF(x, y, drawW, drawH), 0, 0, srcW, srcH,
                       Gdiplus::UnitPixel);
    graphics.Flush();
    return canvas;
}

std::shared_ptr<Gdiplus::Bitmap> ArtworkService::MakePlaceholderFromResource() {
    if (!instance_) return {};
    HRSRC resource = FindResourceW(instance_, MAKEINTRESOURCEW(kPlaceholderResource), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(instance_, resource);
    if (!loaded) return {};
    DWORD size = SizeofResource(instance_, resource);
    void* data = LockResource(loaded);
    if (!data || size == 0) return {};
    std::vector<uint8_t> bytes(static_cast<uint8_t*>(data),
                               static_cast<uint8_t*>(data) + size);
    return Decode(bytes);
}

std::shared_ptr<Gdiplus::Bitmap> ArtworkService::MakePlaceholderFallback() {
    auto canvas = std::make_shared<Gdiplus::Bitmap>(kArtSize, kArtSize, PixelFormat32bppPARGB);
    if (canvas->GetLastStatus() != Gdiplus::Ok) return {};
    Gdiplus::Graphics graphics(canvas.get());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    Gdiplus::LinearGradientBrush background(Gdiplus::Rect(0, 0, kArtSize, kArtSize),
                                            Gdiplus::Color(255, 45, 31, 42),
                                            Gdiplus::Color(255, 24, 27, 44), 45.0f);
    graphics.FillRectangle(&background, 0, 0, kArtSize, kArtSize);
    Gdiplus::SolidBrush glow(Gdiplus::Color(90, 255, 112, 92));
    graphics.FillEllipse(&glow, -80, -90, 340, 340);
    Gdiplus::SolidBrush violet(Gdiplus::Color(80, 112, 84, 214));
    graphics.FillEllipse(&violet, 260, 270, 340, 340);
    Gdiplus::SolidBrush disc(Gdiplus::Color(220, 14, 16, 23));
    graphics.FillEllipse(&disc, 156, 156, 200, 200);
    Gdiplus::SolidBrush accent(Gdiplus::Color(240, 255, 148, 110));
    const int heights[] = {22, 48, 78, 108, 78, 48, 22};
    for (int i = 0; i < 7; ++i) {
        float h = static_cast<float>(heights[i]);
        float x = 208.0f + i * 16.0f;
        float y = 256.0f - h * 0.5f;
        graphics.FillRectangle(&accent, x, y, 8.0f, h);
    }
    return canvas;
}

std::shared_ptr<Gdiplus::Bitmap> ArtworkService::Placeholder() {
    if (placeholder_) return placeholder_;
    placeholder_ = MakePlaceholderFromResource();
    if (!placeholder_) placeholder_ = MakePlaceholderFallback();
    return placeholder_;
}

void ArtworkService::Remember(const std::wstring& key,
                              const std::shared_ptr<Gdiplus::Bitmap>& bitmap) {
    if (!bitmap || key.empty()) return;
    auto found = cache_.find(key);
    if (found != cache_.end()) {
        found->second = bitmap;
        return;
    }
    cache_[key] = bitmap;
    order_.push_back(key);
    while (order_.size() > kCacheLimit) {
        cache_.erase(order_.front());
        order_.pop_front();
    }
}

std::shared_ptr<Gdiplus::Bitmap> ArtworkService::LoadFor(const std::wstring& key,
                                                         const ArtworkBlob& blob) {
    if (!key.empty()) {
        auto found = cache_.find(key);
        if (found != cache_.end()) return found->second;
    }
    if (!blob.Valid()) return Placeholder();
    auto decoded = Decode(blob.bytes);
    if (!decoded) return Placeholder();
    Remember(key, decoded);
    return decoded;
}

void ArtworkService::Clear() {
    cache_.clear();
    order_.clear();
}

}  // namespace mk
