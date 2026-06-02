# Bundled fonts

These fonts ship next to the executable so the app renders identically on every
OS (postmarketOS/Alpine, Fedora, Windows/MSYS2, macOS) without depending on
system-installed fonts. `FontManager` loads them via `SDL_GetBasePath()` and
only falls back to system font locations if the bundled copy is missing.

> Fonts are the **only** binary assets in this repo. The "no image assets" rule
> still holds — every visual is text, emoji, or a code-drawn shape; these files
> are just the glyph data that makes text and color emoji renderable anywhere.

| File | Purpose | License | Upstream |
|------|---------|---------|----------|
| `DejaVuSans.ttf` | Latin/UI text | DejaVu Fonts License (free, redistributable; derived from Bitstream Vera) | https://dejavu-fonts.github.io/ |
| `NotoColorEmoji.ttf` | Color emoji (attached as an SDL_ttf fallback) | SIL Open Font License 1.1 | https://github.com/googlefonts/noto-emoji |

Both licenses permit redistribution and bundling. They are not tracked by Git
LFS: the files never change, so a one-time commit is cheaper than the tooling
friction LFS would add to every clone.
