# Offline Games 🎮

A collection of small, fully-offline games for the **PinePhone**
(postmarketOS + sxmo-de-sway) — inspired by JindoBlu *Offline Games*.

**No image assets at all.** Every pixel is text, emoji, or a shape drawn from
code, so the whole thing is tiny and resolution-independent. Written in
**C++20** with **SDL3** and **SDL3_ttf**.

| | |
|---|---|
| Language | C++20 |
| Graphics | SDL3 (`SDL_Renderer` primitives) + SDL3_ttf for text & color emoji |
| Target | postmarketOS phone + Linux/Windows desktop, portrait 720×1440 |
| Assets | no images — text & emoji only (fonts bundled in `assets/fonts/`) |

## Build & run

The project is cross-platform: C++20 + SDL3 + SDL3_ttf, with the fonts bundled
in `assets/fonts/` so it renders identically on the phone, on a Linux desktop,
and on Windows without installing any fonts. Install the toolchain + SDL3 for
your platform:

```sh
# Alpine / postmarketOS
sudo apk add cmake samurai sdl3-dev sdl3_ttf-dev clang-extra-tools

# Fedora (40+)
sudo dnf install cmake ninja-build gcc-c++ SDL3-devel SDL3_ttf-devel clang-tools-extra

# Windows — MSYS2 UCRT64 shell (no MSVC needed)
pacman -S mingw-w64-ucrt-x86_64-{toolchain,cmake,ninja,SDL3,SDL3_ttf}
```

Then, on any platform:

```sh
cmake --preset dev          # presets: debug | release | dev
cmake --build build/dev
./build/dev/offline-games    # Windows: build/dev/offline-games.exe
```

- **`release`** — optimized build to deploy on the phone.
- **`dev`** — Debug with warnings-as-errors (`WARNINGS_AS_ERRORS=ON`); use it
  while developing. Warning flags are compiler-specific, so GCC, Clang and MSVC
  all build cleanly.

The build copies `assets/fonts/` next to the executable; `FontManager` loads
those first and only falls back to system fonts if the bundle is missing.

On the PinePhone under sxmo-de-sway, build the release preset and launch it on
the next unused numeric workspace:

```sh
cmake --preset release
cmake --build build/release
scripts/run-phone-next-workspace.sh --release
```

Without a preset flag, the script uses the first executable it finds in
`build/release`, `build/dev`, then `build/debug`.

Run the tests (pure game logic, no display needed):

```sh
ctest --test-dir build/dev
```

Check style / static analysis:

```sh
clang-format -i $(find src tests \( -name '*.cpp' -o -name '*.hpp' \))
clang-tidy -p build/dev src/games/tictactoe/TicTacToeBoard.cpp   # etc.
```

## What's included

- A small **engine**: scene stack, unified touch/mouse input, a code-drawing
  `Canvas`, and an emoji-capable font manager.
- A **main menu** listing the available games.
- **Tic-Tac-Toe** (❌ vs ⭕) as the first, fully-playable game and the reference
  pattern for adding more.

## Adding a game

The architecture and the step-by-step recipe live in [`CLAUDE.md`](CLAUDE.md)
and the `cpp-conventions` skill under `.claude/skills/`. The short version:
add a pure (SDL-free, testable) logic class, a `Scene` that renders it, one
entry in `src/games/GameRegistry.cpp`, and a test.

## Controls

Touch (or click) to play. Each game has an on-screen **← Back** button to
return to the menu.

## License

TBD.
