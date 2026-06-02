# offline-games

A collection of small, offline games for the **PinePhone** (postmarketOS +
sxmo-de-sway), inspired by JindoBlu *Offline Games*. Hard constraint: **no image
assets** — every visual is text, emoji, or a shape drawn from code. Built in
**C++20 with SDL3 + SDL3_ttf**.

> When writing C++ here, follow the **`cpp-conventions` skill** (in
> `.claude/skills/`). It is the source of truth for style and the "how to add a
> game" recipe; this file is the architectural map.

## Build, run, test

```sh
# One-time deps (Alpine / postmarketOS):
#   sudo apk add cmake samurai sdl3-dev sdl3_ttf-dev clang-extra-tools
# Cross-platform (Fedora, Windows/MSYS2, …): see README.md → Build & run.
cmake --preset dev            # or: debug / release
cmake --build build/dev
./build/dev/offline-games     # run
ctest --test-dir build/dev    # unit tests (pure game logic)
```

`release` is the optimized preset to deploy on the phone. `dev` is Debug with
warnings-as-errors and is what CI/local checks should use. The codebase is
cross-platform (Linux/Windows/macOS, GCC/Clang/MSVC); fonts are bundled in
`assets/fonts/` so no system fonts are required.

## Architecture

The whole app is a stack of **`Scene`s**. The menu sits at the bottom; opening a
game pushes its scene on top; "back" pops it. Each frame the top scene gets
`handleInput → update(dt) → render`.

```
src/
  main.cpp            entry point: build App, push MenuScene, run
  core/
    App               owns SDL window/renderer + the game loop; converts SDL
                      events into logical-coordinate PointerEvents
    Scene             abstract base for menu and every game
    SceneManager      deferred push/pop/replace stack of scenes
    Canvas            the ONLY thing that touches SDL_Renderer; draws shapes +
                      text/emoji. No images.
    FontManager       caches TTF fonts; attaches Noto Color Emoji as a fallback
                      so one render call mixes text and color emoji. Loads the
                      bundled fonts (assets/fonts/, shipped next to the exe via
                      SDL_GetBasePath) first, then falls back to system fonts
    Sdl.hpp           unique_ptr deleters for SDL/TTF handles (RAII everywhere)
    Layout.hpp        the fixed 720x1440 logical canvas (PinePhone-shaped)
    Color.hpp         shared named palette
    Input.hpp         PointerEvent (touch + mouse unified) + hitTest
  ui/                 reusable widgets: Button, Label
  games/
    GameInfo / GameRegistry   catalog the menu reads; one entry per game
    tictactoe/        reference game (pure board logic + scene)
  scenes/
    MenuScene         lists gameRegistry() as tappable entries
tests/                assert-based CTest exes for the pure logic classes
```

### Key decisions
- **Resolution independence**: scenes draw in a fixed 720×1440 logical space;
  SDL's logical presentation letterboxes it to the real screen (full-screen on
  the phone, a half-size window on a desktop). Input is converted back into the
  same logical space, so device differences never reach game code.
- **Touch + mouse unified**: `App` collapses finger and left-mouse events into
  one `PointerEvent` (Down/Up) in logical pixels.
- **Logic ≠ rendering**: game rules are pure, SDL-free, and unit-tested; scenes
  render them. This is the single most important pattern to preserve.

## Adding a game
See the **`cpp-conventions` skill** → "Adding a game". In short: pure logic
class + a `Scene` + one `GameRegistry` entry + a test, then add the `.cpp`s to
the `engine` target.

## Not yet done (good next steps)
More games (Connect Four, Dots & Boxes…), a scrollable menu when the list grows,
score/settings persistence, sound, and an sxmo `.desktop` launcher entry.
