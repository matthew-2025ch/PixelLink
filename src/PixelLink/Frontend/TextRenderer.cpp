#include <PixelLink/Frontend/TextRenderer.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace PixelLink::Frontend {
TextRenderer::~TextRenderer() {
    for (auto& [key, label] : cache_) SDL_DestroyTexture(label.texture);
}

TextRenderer::Label TextRenderer::MakeLabel(const std::string& text, const int size, const int maxWidth) {
    Label label;
#ifdef _WIN32
    const auto length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) return label;
    std::vector<wchar_t> wide(static_cast<std::size_t>(length));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), length);
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return label;
    HFONT font = CreateFontW(-size, 0, 0, 0, size >= 26 ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    if (!font) { DeleteDC(dc); return label; }
    auto oldFont = SelectObject(dc, font);
    SIZE extent{};
    GetTextExtentPoint32W(dc, wide.data(), length - 1, &extent);
    label.width = std::max(1, std::min(maxWidth, static_cast<int>(extent.cx) + 2));
    label.height = std::max(size + 8, static_cast<int>(extent.cy) + 4);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = label.width;
    info.bmiHeader.biHeight = -label.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap && pixels) {
        auto oldBitmap = SelectObject(dc, bitmap);
        std::fill_n(static_cast<std::uint32_t*>(pixels), label.width * label.height, 0);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rect{0, 0, label.width, label.height};
        DrawTextW(dc, wide.data(), length - 1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        GdiFlush();
        auto* rgba = static_cast<std::uint32_t*>(pixels);
        for (int i = 0; i < label.width * label.height; ++i) {
            const auto coverage = rgba[i] & 0xFF;
            rgba[i] = (coverage << 24) | 0xFFFFFF;
        }
        label.texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STATIC, label.width, label.height);
        if (label.texture) {
            if (!SDL_UpdateTexture(label.texture, nullptr, pixels, label.width * 4) ||
                !SDL_SetTextureBlendMode(label.texture, SDL_BLENDMODE_BLEND)) {
                SDL_DestroyTexture(label.texture); label.texture = nullptr;
            }
        }
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
#else
    (void)text; (void)size; (void)maxWidth;
#endif
    return label;
}

void TextRenderer::Draw(const std::string& text, const float x, const float y, const int size,
                         const SDL_Color color, const float maxWidth) {
    if (text.empty()) return;
    const auto key = std::to_string(size) + ":" + std::to_string(static_cast<int>(maxWidth)) + ":" + text;
    auto [it, inserted] = cache_.try_emplace(key);
    if (inserted) it->second = MakeLabel(text, size, static_cast<int>(maxWidth));
    auto& label = it->second;
    label.used = frame_;
    if (label.texture) {
        SDL_SetTextureColorMod(label.texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(label.texture, color.a);
        const SDL_FRect rect{x, y, static_cast<float>(label.width), static_cast<float>(label.height)};
        SDL_RenderTexture(renderer_, label.texture, nullptr, &rect);
    } else {
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        SDL_SetRenderScale(renderer_, size / 10.0f, size / 10.0f);
        SDL_RenderDebugText(renderer_, x * 10 / size, y * 10 / size, text.c_str());
        SDL_SetRenderScale(renderer_, 1, 1);
    }
}

void TextRenderer::EndFrame() {
    // Bound the cache when users scroll/search through a large ROM collection.
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (frame_ > it->second.used + 120) {
            SDL_DestroyTexture(it->second.texture);
            it = cache_.erase(it);
        } else ++it;
    }
}
}
