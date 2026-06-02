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
| Target | postmarketOS / Alpine, portrait touchscreen (720×1440) |
| Assets | none — emoji & text only |

## Build & run

Install dependencies (Alpine / postmarketOS):

```sh
sudo apk add sdl3-dev sdl3_ttf-dev clang-extra-tools font-dejavu
```

Then:

```sh
cmake --preset dev          # presets: debug | release | dev
cmake --build build/dev
./build/dev/offline-games
```

- **`release`** — optimized build to deploy on the phone.
- **`dev`** — Debug with warnings-as-errors; use it while developing.

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
