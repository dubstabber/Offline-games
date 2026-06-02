---
name: cpp-conventions
description: >
  Coding conventions for the offline-games project (C++20, SDL3, emoji/text
  only). Use whenever writing or modifying C++ in this repo, adding a new game,
  or touching the engine/UI/rendering code.
---

# offline-games C++ conventions

A touch-first collection of small games for the PinePhone (postmarketOS +
sxmo-de-sway). **No image assets, ever** — every visual is text, emoji, or a
code-drawn shape. Keep the code clean, small, and easy to extend.

## Golden rules

1. **No images.** Visuals come only from `Canvas` (shapes) and font glyphs
   (text + color emoji). Never add a PNG/JPG/SVG or an image-loading path.
2. **Logic is separate from rendering.** Game rules live in a pure class with
   **no SDL include** (e.g. `TicTacToeBoard`) so they are unit-testable. The
   matching `Scene` does all drawing/input and owns the logic object.
3. **RAII for every resource.** SDL/TTF handles are held through the smart
   pointers in `core/Sdl.hpp` (`WindowPtr`, `TexturePtr`, `FontPtr`, …). Never
   call `SDL_Destroy*`/`TTF_Close*` by hand and never use raw `new`/`delete`.
4. **Draw in logical coordinates.** Everything is laid out in the fixed
   720×1440 canvas from `core/Layout.hpp`; SDL letterboxes it to the real
   screen. Don't query window pixels in scenes. Touch targets ≥ `kMinTouchSize`.

## Structure & naming

- Header/impl split: `.hpp` declares, `.cpp` defines. One class per file, file
  named after the class.
- Namespace `og` for everything. File-local helpers go in an anonymous
  namespace in the `.cpp`.
- Types/enums `PascalCase`; functions & variables `camelCase`; data members
  trailing underscore `member_`; constants `kPascalCase`; macros — none.
- Includes ordered project → SDL → std (clang-format regroups them; just run
  it). Prefer forward declarations in headers over includes.
- Keep functions small and single-purpose. Mark methods `const`, `constexpr`,
  and `[[nodiscard]]` where they apply. `enum class`, never bare `enum`.

## Adding a game (the pattern to copy)

1. Create `src/games/<name>/`.
2. Add a **pure logic class** `<Name>.{hpp,cpp}` — no SDL, fully testable.
3. Add a **`<Name>Scene` : `Scene`** that owns the logic, renders it via
   `Canvas`, and turns `PointerEvent`s into moves. Take `SceneManager&` and use
   `manager.pop()` for "back".
4. Register it: append one `GameInfo{ id, title, emoji, create }` in
   `src/games/GameRegistry.cpp` (include the scene header). Nothing else changes.
5. Add the new `.cpp` files to the `engine` target in `CMakeLists.txt`.
6. Add `tests/test_<name>.cpp` exercising the logic class and register it in
   `tests/CMakeLists.txt`.

Use `TicTacToeBoard` / `TicTacToeScene` as the worked example.

## Before you finish

Always run, from the repo root:

```sh
cmake --build build/dev                       # must be clean (warnings are errors)
ctest --test-dir build/dev                    # logic tests pass
clang-format -i <changed files>               # enforced formatting
clang-tidy -p build/dev <changed .cpp files>  # no new warnings
```

If you changed game logic, a test must cover it. If a check can't pass, say so —
don't silence it by weakening `.clang-tidy`/warnings without calling it out.
