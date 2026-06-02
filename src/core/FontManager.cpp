#include "core/FontManager.hpp"

#include <cmath>
#include <initializer_list>
#include <SDL3/SDL.h>

namespace og {
namespace {

// First existing path wins. The DejaVu/Noto locations are the Alpine /
// postmarketOS defaults; the lists make the binary portable across dev box and
// phone without bundling any files.
const char* firstExistingPath(std::initializer_list<const char*> candidates) {
    for (const char* path : candidates) {
        if (path != nullptr && SDL_GetPathInfo(path, nullptr)) {
            return path;
        }
    }
    return nullptr;
}

} // namespace

FontManager::FontManager() {
    textFontPath_ = firstExistingPath({
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    });
    emojiFontPath_ = firstExistingPath({
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/twemoji/Twemoji.ttf",
    });

    if (textFontPath_ == nullptr) {
        SDL_Log("FontManager: no text font found; text will not render.");
    }
}

FontPtr FontManager::openFont(const char* path, float size) {
    if (path == nullptr) {
        return nullptr;
    }
    FontPtr font{TTF_OpenFont(path, size)};
    if (!font) {
        SDL_Log("FontManager: failed to open %s: %s", path, SDL_GetError());
    }
    return font;
}

TTF_Font* FontManager::fontForSize(float pixelSize) {
    const int size = static_cast<int>(std::lround(pixelSize));
    for (const Entry& entry : cache_) {
        if (entry.size == size) {
            return entry.text.get();
        }
    }

    Entry entry;
    entry.size = size;
    entry.text = openFont(textFontPath_, pixelSize);
    if (!entry.text) {
        return nullptr;
    }

    // Attach color emoji as a fallback. The emoji font is stored alongside the
    // text font so its lifetime matches the fallback link held by SDL_ttf.
    entry.emoji = openFont(emojiFontPath_, pixelSize);
    if (entry.emoji) {
        TTF_AddFallbackFont(entry.text.get(), entry.emoji.get());
    }

    cache_.push_back(std::move(entry));
    return cache_.back().text.get();
}

} // namespace og
