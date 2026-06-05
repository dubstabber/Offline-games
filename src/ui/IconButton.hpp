#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace og {

class Canvas;
struct PointerEvent;

// A circular, touch-sized chrome button drawn entirely from code: a disc in the
// theme's chrome color with either a drawn back-chevron or a centered glyph
// (emoji/text). Every game's top-corner back button and the Block Fill /
// Minesweeper reset buttons are this widget. It tracks its own press state and
// fires onTap when a press both starts and ends inside its (square) touch box —
// the exact behavior of the per-scene handlers it replaces. The disc and chevron
// colors are read from the active theme each frame, so dark mode recolors it
// live; the button itself shows no press-down tint (matching today's chrome).
class IconButton {
public:
    enum class Icon : std::uint8_t { Chevron, Glyph };

    // Centered on (cx, cy) with the given radius, all in logical pixels.
    IconButton(Icon icon, float cx, float cy, float radius);

    // For Icon::Glyph: the UTF-8 glyph (e.g. a reset emoji) and its draw height.
    void setGlyph(std::string glyph, float glyphSize);
    void setOnTap(std::function<void()> onTap) { onTap_ = std::move(onTap); }

    // Returns true if the event was consumed by this button.
    bool handleInput(const PointerEvent& event);
    void render(Canvas& canvas) const;

private:
    Icon icon_;
    float cx_;
    float cy_;
    float radius_;
    std::string glyph_;
    float glyphSize_ = 54.0F;
    bool pressed_ = false;
    std::function<void()> onTap_;
};

} // namespace og
