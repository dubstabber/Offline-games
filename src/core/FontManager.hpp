#pragma once

#include "core/Sdl.hpp"

#include <vector>

namespace og {

// Owns the TTF fonts. We keep one text font per size and attach Noto Color
// Emoji as a fallback so a single render call can mix letters and color emoji
// (e.g. a button labelled "Play  🎮"). Fonts are opened lazily and cached by
// rounded pixel size.
class FontManager {
public:
    FontManager();

    // Returns a font for the requested logical pixel size, or nullptr if no
    // usable font could be opened. Ownership stays with the manager.
    TTF_Font* fontForSize(float pixelSize);

private:
    struct Entry {
        int size = 0;
        FontPtr text;  // primary font (owns the fallback link below)
        FontPtr emoji; // kept alive for as long as `text` references it
    };

    [[nodiscard]] static FontPtr openFont(const char* path, float size);

    const char* textFontPath_ = nullptr;
    const char* emojiFontPath_ = nullptr;
    std::vector<Entry> cache_;
};

} // namespace og
