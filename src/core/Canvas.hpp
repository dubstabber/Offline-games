#pragma once

#include "core/Color.hpp"
#include "core/Sdl.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace og {

class FontManager;

// The only thing that talks to SDL_Renderer. Everything is drawn from code:
// flat shapes (rects, rounded rects, circles, lines) plus text/emoji glyphs.
// There are no image assets anywhere in the project. All coordinates are in
// logical canvas pixels (see Layout).
class Canvas {
public:
    enum class Align : std::uint8_t { Left, Center, Right };

    struct Size {
        float w = 0.0F;
        float h = 0.0F;
    };

    Canvas(SDL_Renderer* renderer, FontManager& fonts);

    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }

    // ---- Shapes -------------------------------------------------------------
    void clear(Color color);
    void fillRect(float x, float y, float w, float h, Color color);
    void fillRoundedRect(float x, float y, float w, float h, float radius, Color color);
    void fillCircle(float cx, float cy, float radius, Color color);
    void line(float x1, float y1, float x2, float y2, float thickness, Color color);

    // A single colored vertex in logical canvas pixels, used by the polygon and
    // mesh primitives below so games can describe filled geometry (e.g. extruded
    // hex prisms) without ever touching SDL types directly.
    struct Vertex {
        float x = 0.0F;
        float y = 0.0F;
        Color color{};
    };

    // Filled convex polygon (>= 3 corners), triangulated as a fan from corners[0]
    // in one SDL_RenderGeometry call. Corners must be given in boundary order.
    void fillConvexPolygon(std::span<const Vertex> corners);

    // Draw an arbitrary triangle mesh (indices into verts) in a single
    // SDL_RenderGeometry call. The batched path: build one big buffer — a whole
    // hex field, say — and submit it at once instead of thousands of small draws,
    // which matters on the PinePhone's draw-call-bound Mali-400.
    void fillMesh(std::span<const Vertex> verts, std::span<const int> indices);

    // ---- Text / emoji -------------------------------------------------------
    // `pixelSize` is the glyph height in logical pixels. `align` positions the
    // text horizontally relative to x; y is always the top edge.
    void text(std::string_view str, float x, float y, float pixelSize, Color color,
              Align align = Align::Left);

    // Convenience: center a single line on (cx, cy).
    void textCentered(std::string_view str, float cx, float cy, float pixelSize, Color color);

    // Draw a glyph (typically a color emoji) centered on (cx, cy) and scaled to
    // fit a `size`-tall box. Color-emoji fonts rasterize at a fixed bitmap strike
    // that ignores the requested point size, so this scales the rendered texture
    // to the size you actually want.
    void emojiCentered(std::string_view str, float cx, float cy, float size);

    Size measure(std::string_view str, float pixelSize);

private:
    struct CachedText {
        TexturePtr texture;
        float w = 0.0F;
        float h = 0.0F;
    };

    const CachedText* rasterize(std::string_view str, float pixelSize, Color color);

    SDL_Renderer* renderer_;
    // Non-owning, never-null dependency owned by App for the Canvas's whole
    // lifetime; a reference documents that better than a pointer. Canvas is
    // intentionally non-copyable, so the deleted assignment is fine.
    FontManager& fonts_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::unordered_map<std::string, CachedText> textCache_;
    std::vector<SDL_Vertex> meshScratch_; // reused by fillMesh to avoid per-frame allocation
};

} // namespace og
