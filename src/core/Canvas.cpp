#include "core/Canvas.hpp"

#include "core/FontManager.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace og {
namespace {

constexpr int kCircleSegments = 32;

void setDrawColor(SDL_Renderer* renderer, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

// The unit-circle ring positions and the triangle-fan index list are the same
// for every disc, so compute them once. fillDisc then only scales+translates the
// ring into a stack buffer — no per-call heap allocation and no per-call trig,
// which is what made circles (every rounded-rect corner, every dot) costly on
// the PinePhone. The values are identical to the previous per-call computation.
struct DiscTables {
    std::array<SDL_FPoint, kCircleSegments + 1> ring{}; // cos/sin at i = 0..kCircleSegments
    std::array<int, static_cast<std::size_t>(kCircleSegments) * 3> indices{};
};

const DiscTables& discTables() {
    static const DiscTables tables = [] {
        DiscTables t;
        for (int i = 0; i <= kCircleSegments; ++i) {
            const float a =
                (static_cast<float>(i) / kCircleSegments) * 2.0F * std::numbers::pi_v<float>;
            t.ring.at(static_cast<std::size_t>(i)) = SDL_FPoint{.x = std::cos(a), .y = std::sin(a)};
        }
        std::size_t n = 0;
        for (int i = 1; i <= kCircleSegments; ++i) {
            t.indices.at(n++) = 0;
            t.indices.at(n++) = i;
            t.indices.at(n++) = i + 1;
        }
        return t;
    }();
    return tables;
}

// Fan of triangles approximating a filled disc, drawn via SDL_RenderGeometry.
void fillDisc(SDL_Renderer* renderer, float cx, float cy, float radius, Color color) {
    if (radius <= 0.0F) {
        return;
    }
    const SDL_FColor fc{.r = static_cast<float>(color.r) / 255.0F,
                        .g = static_cast<float>(color.g) / 255.0F,
                        .b = static_cast<float>(color.b) / 255.0F,
                        .a = static_cast<float>(color.a) / 255.0F};

    const DiscTables& tables = discTables();
    std::array<SDL_Vertex, kCircleSegments + 2> verts{};
    verts.at(0) = SDL_Vertex{.position = SDL_FPoint{.x = cx, .y = cy},
                             .color = fc,
                             .tex_coord = SDL_FPoint{.x = 0.0F, .y = 0.0F}};
    for (int i = 0; i <= kCircleSegments; ++i) {
        const SDL_FPoint& u = tables.ring.at(static_cast<std::size_t>(i));
        verts.at(static_cast<std::size_t>(i) + 1) =
            SDL_Vertex{.position = SDL_FPoint{.x = cx + (u.x * radius), .y = cy + (u.y * radius)},
                       .color = fc,
                       .tex_coord = SDL_FPoint{.x = 0.0F, .y = 0.0F}};
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       tables.indices.data(), static_cast<int>(tables.indices.size()));
}

} // namespace

Canvas::Canvas(SDL_Renderer* renderer, FontManager& fonts) : renderer_(renderer), fonts_(fonts) {}

void Canvas::clear(Color color) {
    setDrawColor(renderer_, color);
    SDL_RenderClear(renderer_);
}

void Canvas::fillRect(float x, float y, float w, float h, Color color) {
    setDrawColor(renderer_, color);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void Canvas::fillRoundedRect(float x, float y, float w, float h, float radius, Color color) {
    const float r = std::min(radius, std::min(w, h) / 2.0F);
    if (r <= 0.0F) {
        fillRect(x, y, w, h, color);
        return;
    }
    // Cross of two rects covers the straight edges; four discs round the corners.
    fillRect(x + r, y, w - (2.0F * r), h, color);
    fillRect(x, y + r, w, h - (2.0F * r), color);
    fillDisc(renderer_, x + r, y + r, r, color);
    fillDisc(renderer_, x + w - r, y + r, r, color);
    fillDisc(renderer_, x + r, y + h - r, r, color);
    fillDisc(renderer_, x + w - r, y + h - r, r, color);
}

void Canvas::fillCircle(float cx, float cy, float radius, Color color) {
    fillDisc(renderer_, cx, cy, radius, color);
}

void Canvas::line(float x1, float y1, float x2, float y2, float thickness, Color color) {
    // Render the segment as a rotated quad so it can have real thickness.
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float len = std::sqrt((dx * dx) + (dy * dy));
    if (len <= 0.0F) {
        return;
    }
    const float nx = (-dy / len) * (thickness / 2.0F);
    const float ny = (dx / len) * (thickness / 2.0F);
    const SDL_FColor fc{static_cast<float>(color.r) / 255.0F, static_cast<float>(color.g) / 255.0F,
                        static_cast<float>(color.b) / 255.0F, static_cast<float>(color.a) / 255.0F};

    const std::array<SDL_Vertex, 4> verts{
        SDL_Vertex{SDL_FPoint{x1 + nx, y1 + ny}, fc, SDL_FPoint{0, 0}},
        SDL_Vertex{SDL_FPoint{x2 + nx, y2 + ny}, fc, SDL_FPoint{0, 0}},
        SDL_Vertex{SDL_FPoint{x2 - nx, y2 - ny}, fc, SDL_FPoint{0, 0}},
        SDL_Vertex{SDL_FPoint{x1 - nx, y1 - ny}, fc, SDL_FPoint{0, 0}},
    };
    const std::array<int, 6> indices{0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer_, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void Canvas::fillMesh(std::span<const Vertex> verts, std::span<const int> indices) {
    if (verts.size() < 3 || indices.size() < 3) {
        return;
    }
    meshScratch_.clear();
    meshScratch_.reserve(verts.size());
    for (const Vertex& v : verts) {
        meshScratch_.push_back(SDL_Vertex{SDL_FPoint{v.x, v.y},
                                          SDL_FColor{static_cast<float>(v.color.r) / 255.0F,
                                                     static_cast<float>(v.color.g) / 255.0F,
                                                     static_cast<float>(v.color.b) / 255.0F,
                                                     static_cast<float>(v.color.a) / 255.0F},
                                          SDL_FPoint{0, 0}});
    }
    SDL_RenderGeometry(renderer_, nullptr, meshScratch_.data(),
                       static_cast<int>(meshScratch_.size()), indices.data(),
                       static_cast<int>(indices.size()));
}

void Canvas::fillConvexPolygon(std::span<const Vertex> corners) {
    if (corners.size() < 3) {
        return;
    }
    // Triangle fan from corner 0: (0,1,2), (0,2,3), … built into a reused buffer
    // so a per-frame polygon (hex prisms, dish ellipses) allocates nothing.
    polyScratch_.clear();
    for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
        polyScratch_.push_back(0);
        polyScratch_.push_back(static_cast<int>(i));
        polyScratch_.push_back(static_cast<int>(i + 1));
    }
    fillMesh(corners, polyScratch_);
}

const Canvas::CachedText* Canvas::rasterize(std::string_view str, float pixelSize, Color color) {
    // Cache key folds in size and color so identical labels reuse one texture
    // instead of re-rasterizing every frame. The key is built into a reused
    // scratch buffer via std::to_chars, so the common case — a cache hit on a
    // HUD label drawn every frame — performs no heap allocation at all.
    keyScratch_.clear();
    std::array<char, 16> num{};
    const auto appendInt = [&](int value) {
        const auto result = std::to_chars(num.data(), num.data() + num.size(), value);
        keyScratch_.append(num.data(), result.ptr);
    };
    appendInt(static_cast<int>(std::lround(pixelSize)));
    keyScratch_ += ':';
    appendInt((color.r << 16) | (color.g << 8) | color.b);
    keyScratch_ += ':';
    keyScratch_.append(str);

    if (auto it = textCache_.find(keyScratch_); it != textCache_.end()) {
        return &it->second;
    }

    TTF_Font* font = fonts_.fontForSize(pixelSize);
    if (font == nullptr) {
        return nullptr;
    }
    SurfacePtr surface{TTF_RenderText_Blended(font, str.data(), str.size(), color)};
    if (!surface) {
        return nullptr;
    }
    TexturePtr texture{SDL_CreateTextureFromSurface(renderer_, surface.get())};
    if (!texture) {
        return nullptr;
    }

    CachedText entry;
    entry.w = static_cast<float>(surface->w);
    entry.h = static_cast<float>(surface->h);
    entry.texture = std::move(texture);
    // Copy the key into the map (keyScratch_ is reused next call, so can't move).
    auto [it, _] = textCache_.emplace(keyScratch_, std::move(entry));
    return &it->second;
}

void Canvas::text(std::string_view str, float x, float y, float pixelSize, Color color,
                  Align align) {
    if (str.empty()) {
        return;
    }
    const CachedText* cached = rasterize(str, pixelSize, color);
    if (cached == nullptr) {
        return;
    }
    float drawX = x;
    if (align == Align::Center) {
        drawX = x - (cached->w / 2.0F);
    } else if (align == Align::Right) {
        drawX = x - cached->w;
    }
    const SDL_FRect dst{drawX, y, cached->w, cached->h};
    SDL_RenderTexture(renderer_, cached->texture.get(), nullptr, &dst);
}

void Canvas::textCentered(std::string_view str, float cx, float cy, float pixelSize, Color color) {
    if (str.empty()) {
        return;
    }
    // Rasterize once (at the real color) and place it centered on (cx, cy). The
    // old path called measure() then text(), building the cache key twice and
    // caching an extra colors::text-tinted copy; glyph metrics are
    // color-independent, so this draws the exact same pixels with half the work.
    const CachedText* cached = rasterize(str, pixelSize, color);
    if (cached == nullptr) {
        return;
    }
    const SDL_FRect dst{
        .x = cx - (cached->w / 2.0F), .y = cy - (cached->h / 2.0F), .w = cached->w, .h = cached->h};
    SDL_RenderTexture(renderer_, cached->texture.get(), nullptr, &dst);
}

void Canvas::emojiCentered(std::string_view str, float cx, float cy, float size) {
    if (str.empty() || size <= 0.0F) {
        return;
    }
    // Rasterize once at a fixed, high-ish size (the emoji strike is fixed anyway),
    // then scale the texture down to the requested size.
    constexpr float kRasterSize = 72.0F;
    const CachedText* cached = rasterize(str, kRasterSize, colors::white);
    if (cached == nullptr || cached->w <= 0.0F || cached->h <= 0.0F) {
        return;
    }
    const float scale = size / std::max(cached->w, cached->h);
    const float w = cached->w * scale;
    const float h = cached->h * scale;
    const SDL_FRect dst{.x = cx - (w / 2.0F), .y = cy - (h / 2.0F), .w = w, .h = h};
    SDL_RenderTexture(renderer_, cached->texture.get(), nullptr, &dst);
}

Canvas::Size Canvas::measure(std::string_view str, float pixelSize) {
    const CachedText* cached = rasterize(str, pixelSize, colors::text);
    if (cached == nullptr) {
        return {};
    }
    return {.w = cached->w, .h = cached->h};
}

} // namespace og
