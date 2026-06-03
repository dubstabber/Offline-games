#pragma once

#include <memory>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

// RAII wrappers for the C-style SDL resource handles. Owning these through
// std::unique_ptr means cleanup is automatic and exception-safe; no module in
// this project should ever call the matching SDL_Destroy*/TTF_Close* directly.
namespace og {

struct SdlWindowDeleter {
    void operator()(SDL_Window* w) const noexcept { SDL_DestroyWindow(w); }
};
struct SdlRendererDeleter {
    void operator()(SDL_Renderer* r) const noexcept { SDL_DestroyRenderer(r); }
};
struct SdlTextureDeleter {
    void operator()(SDL_Texture* t) const noexcept { SDL_DestroyTexture(t); }
};
struct SdlSurfaceDeleter {
    void operator()(SDL_Surface* s) const noexcept { SDL_DestroySurface(s); }
};
struct TtfFontDeleter {
    void operator()(TTF_Font* f) const noexcept { TTF_CloseFont(f); }
};
// For buffers SDL hands back malloc'd (SDL_GetPrefPath, SDL_LoadFile, …) that the
// caller must release with SDL_free. Takes void* so it owns char*/byte buffers.
struct SdlFreeDeleter {
    void operator()(void* p) const noexcept { SDL_free(p); }
};

using WindowPtr = std::unique_ptr<SDL_Window, SdlWindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, SdlRendererDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SdlSurfaceDeleter>;
using FontPtr = std::unique_ptr<TTF_Font, TtfFontDeleter>;
using SdlCharPtr = std::unique_ptr<char, SdlFreeDeleter>;

} // namespace og
