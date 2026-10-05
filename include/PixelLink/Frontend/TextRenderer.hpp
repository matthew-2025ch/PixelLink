#pragma once
#include <SDL3/SDL.h>
#include <map>
#include <string>

namespace PixelLink::Frontend {
// Rasterizes the Windows system font to SDL textures, so Chinese UI and ROM
// names work without distributing another font or runtime DLL.
class TextRenderer {
public:
    explicit TextRenderer(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    void Draw(const std::string& text, float x, float y, int size,
              SDL_Color color, float maxWidth = 1000);
    void BeginFrame() { ++frame_; }
    void EndFrame();
private:
    struct Label { SDL_Texture* texture{}; int width{}; int height{}; std::uint64_t used{}; };
    SDL_Renderer* renderer_;
    std::map<std::string, Label> cache_;
    std::uint64_t frame_ = 0;
    Label MakeLabel(const std::string& text, int size, int maxWidth);
};
}
