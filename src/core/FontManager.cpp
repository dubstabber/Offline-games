#include "core/FontManager.hpp"

#include <cmath>
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace og {
namespace {

// First existing path wins; empty candidates are skipped. The bundled fonts
// (shipped next to the executable) are listed first so the app looks identical
// on every OS; the system locations are fallbacks for when the bundle is gone.
std::string firstExistingPath(const std::vector<std::string>& candidates) {
    for (const std::string& path : candidates) {
        if (!path.empty() && SDL_GetPathInfo(path.c_str(), nullptr)) {
            return path;
        }
    }
    return {};
}

// Directory the executable lives in, with a trailing separator, or empty if SDL
// cannot determine it. The returned string is owned by SDL and must not be freed.
std::string bundledFontDir() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    return std::string(base) + "assets/fonts/";
}

std::string bundled(const std::string& dir, const char* file) {
    return dir.empty() ? std::string{} : dir + file;
}

} // namespace

FontManager::FontManager() {
    const std::string dir = bundledFontDir();

    textFontPath_ = firstExistingPath({
        bundled(dir, "DejaVuSans.ttf"),
        // Linux: Alpine/postmarketOS, Arch, Fedora, Debian/Ubuntu layouts.
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        // Windows.
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        // macOS.
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
    });
    emojiFontPath_ = firstExistingPath({
        bundled(dir, "NotoColorEmoji.ttf"),
        // Linux color-emoji locations across distros.
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/google-noto-emoji/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/twemoji/Twemoji.ttf",
        // Windows color emoji (COLR/CPAL).
        "C:/Windows/Fonts/seguiemj.ttf",
        // macOS color emoji.
        "/System/Library/Fonts/Apple Color Emoji.ttc",
    });

    if (textFontPath_.empty()) {
        SDL_Log("FontManager: no text font found; text will not render.");
    }
}

FontPtr FontManager::openFont(const std::string& path, float size) {
    if (path.empty()) {
        return nullptr;
    }
    FontPtr font{TTF_OpenFont(path.c_str(), size)};
    if (!font) {
        SDL_Log("FontManager: failed to open %s: %s", path.c_str(), SDL_GetError());
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
