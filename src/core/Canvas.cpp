#include "core/Canvas.hpp"

#include "core/FontManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace og {
namespace {

constexpr int kCircleSegments = 32;

void setDrawColor(SDL_Renderer* renderer, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

// Fan of triangles approximating a filled disc, drawn via SDL_RenderGeometry.
void fillDisc(SDL_Renderer* renderer, float cx, float cy, float radius, Color color) {
    if (radius <= 0.0F) {
        return;
    }
    const SDL_FColor fc{static_cast<float>(color.r) / 255.0F, static_cast<float>(color.g) / 255.0F,
                        static_cast<float>(color.b) / 255.0F, static_cast<float>(color.a) / 255.0F};

    std::vector<SDL_Vertex> verts;
    verts.reserve(kCircleSegments + 2);
    verts.push_back(SDL_Vertex{SDL_FPoint{cx, cy}, fc, SDL_FPoint{0, 0}});
    for (int i = 0; i <= kCircleSegments; ++i) {
        const float t =
            (static_cast<float>(i) / kCircleSegments) * 2.0F * std::numbers::pi_v<float>;
        verts.push_back(
            SDL_Vertex{SDL_FPoint{cx + (std::cos(t) * radius), cy + (std::sin(t) * radius)}, fc,
                       SDL_FPoint{0, 0}});
    }

    std::vector<int> indices;
    indices.reserve(static_cast<std::size_t>(kCircleSegments) * 3);
    for (int i = 1; i <= kCircleSegments; ++i) {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
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

const Canvas::CachedText* Canvas::rasterize(std::string_view str, float pixelSize, Color color) {
    // Cache key folds in size and color so identical labels reuse one texture
    // instead of re-rasterizing every frame.
    std::string key;
    key.reserve(str.size() + 16);
    key += std::to_string(static_cast<int>(std::lround(pixelSize)));
    key += ':';
    key += std::to_string((color.r << 16) | (color.g << 8) | color.b);
    key += ':';
    key.append(str);

    if (auto it = textCache_.find(key); it != textCache_.end()) {
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
    auto [it, _] = textCache_.emplace(std::move(key), std::move(entry));
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
    const Size size = measure(str, pixelSize);
    text(str, cx, cy - (size.h / 2.0F), pixelSize, color, Align::Center);
}

Canvas::Size Canvas::measure(std::string_view str, float pixelSize) {
    const CachedText* cached = rasterize(str, pixelSize, colors::text);
    if (cached == nullptr) {
        return {};
    }
    return {.w = cached->w, .h = cached->h};
}

} // namespace og
