#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace og {

class Canvas;
struct PointerEvent;

// A circular, touch-sized chrome button: a disc in the theme's chrome color with
// a centered color emoji — the standard back arrow (Icon::Back) or any glyph the
// scene sets (Icon::Glyph). Every game's top-corner back button and the undo /
// reset / d-pad buttons are this widget. It tracks its own press state and fires
// onTap when a press both starts and ends inside its (square) touch box. The disc
// color is read from the active theme each frame, so dark mode recolors it live;
// the button itself shows no press-down tint (matching the app's chrome).
// Glyphs must be color emoji: plain text is rasterized white and vanishes on the
// light theme's white disc (Canvas draws emoji-only strings with the emoji font).
class IconButton {
public:
    enum class Icon : std::uint8_t { Back, Glyph };

    // Centered on (cx, cy) with the given radius, all in logical pixels.
    IconButton(Icon icon, float cx, float cy, float radius);

    // For Icon::Glyph: the UTF-8 emoji (e.g. a reset emoji) and its draw height.
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
