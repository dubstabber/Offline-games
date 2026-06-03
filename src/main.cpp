#include "core/App.hpp"
#include "core/Settings.hpp"
#include "scenes/MenuScene.hpp"

#include <memory>
#include <SDL3/SDL_main.h>

// SDL_main.h remaps `main` to the platform entry point (e.g. WinMain on
// Windows), so the signature must be the argc/argv form even though we ignore
// the arguments.
int main(int /*argc*/, char* /*argv*/[]) {
    og::App app;
    if (!app.init()) {
        return 1;
    }
    // Force the one-time load now (after SDL_Init) so the first frame already
    // reflects the saved theme/options instead of loading lazily mid-render.
    (void)og::settings();
    app.scenes().push(std::make_unique<og::MenuScene>(app.scenes()));
    app.scenes().applyPending();
    app.run();
    return 0;
}
