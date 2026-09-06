#pragma once

#include "core/Color.hpp"

namespace og {

// The subset of colors that flip between light and dark mode. Everything else
// (player reds/cyans, difficulty colors, menu card accents, emoji, white, the
// game-over overlay) is identity/accent and stays in colors:: unchanged across
// themes — those read fine on dark charcoal and keep each game recognizable.
struct Theme {
    // Chrome shared by the menu, settings, and difficulty screens.
    Color appBg;       // App's per-frame clear (scenes usually clear over it)
    Color menuBg;      // menu / settings / difficulty background
    Color topBar;      // menu top bar fill
    Color barLine;     // separator under the top bar
    Color cardFill;    // menu game card
    Color cardPressed; // menu game card while pressed
    Color primaryText; // text on always-dark surfaces (menu cards, game fields)
    Color titleText;   // screen titles (flips with the chrome background)
    Color bodyText;    // labels/body text on the chrome background (flips)
    Color mutedText;   // secondary text (descriptions, value labels)
    Color backCircle;  // back-button disc
    Color sliderTrack; // unfilled slider track

    // Tic-Tac-Toe.
    Color tttBg;       // board background
    Color tttPanel;    // scoreboard panel
    Color tttTab;      // difficulty tab behind the panel
    Color tttGridLine; // the `#` grid (inverts to light on a dark board)

    // Tap Match.
    Color tmBg;         // playfield background
    Color tmTileLight;  // free/accessible tile card (light, backs the emoji)
    Color tmTileDim;    // covered tile card
    Color tmTileEdge;   // tile border between stacked tiles
    Color tmHolder;     // holder bar panel
    Color tmSlot;       // recessed holder slot
    Color tmStatusText; // "N left" status line

    // Minesweeper.
    Color msField;       // board + screen background (dark slate; numbers sit on it)
    Color msTileCovered; // covered cell card (stays light to read in both themes)
    Color msTileEdge;    // covered cell bottom shadow / separation
    Color msPanel;       // CURRENT STREAK / ALL TIME HUD panels

    // Block Fill.
    Color bfField; // board + screen background (dark; the rope/cells sit on it)
    Color bfCell;  // empty playable cell card (gray)

    // Snake.
    Color snakeField;  // arena background (the dark "void"; snakes/food sit on it)
    Color snakeBorder; // world-edge frame marking the death boundary

    // 2048.
    Color tfeBg;    // screen background
    Color tfeBoard; // board panel + score cards (the tiles' warm grey frame)
    Color tfeCell;  // empty cell

    // Memory.
    Color memBgYou;        // table on the player's turn (the screen crossfades)
    Color memBgBot;        // table on the bot's turn
    Color memPanel;        // scoreboard panel
    Color memCardBack;     // face-down card
    Color memCardBackRing; // the ring pattern on a face-down card
    Color memCardFace;     // face-up card (light, backs the emoji)

    // Solitaire.
    Color solTable;         // the felt
    Color solSlot;          // empty pile outline (translucent over the felt)
    Color solCardFace;      // face-up card (light, backs the black/red ink)
    Color solCardBack;      // face-down card
    Color solCardBackInner; // the frame pattern on a face-down card
};

// Light theme = the project's current colors (verbatim, so default builds look
// identical until dark mode is toggled).
inline constexpr Theme kLight{
    .appBg = colors::background,
    .menuBg = colors::cream,
    .topBar = colors::white,
    .barLine = rgb(226, 206, 180),
    .cardFill = rgb(40, 38, 52),
    .cardPressed = rgb(58, 55, 74),
    .primaryText = colors::text,
    .titleText = colors::gridBlack,
    .bodyText = rgb(66, 72, 82), // dark slate, like the original's labels
    .mutedText = rgb(96, 84, 80),
    .backCircle = colors::white,
    .sliderTrack = rgb(225, 210, 190),

    .tttBg = colors::coral,
    .tttPanel = colors::panelBrown,
    .tttTab = rgb(92, 68, 68),
    .tttGridLine = colors::gridBlack,

    .tmBg = colors::tapMatchMaroon,
    .tmTileLight = colors::tapMatchTileLight,
    .tmTileDim = colors::tapMatchTileDim,
    .tmTileEdge = colors::tapMatchTileEdge,
    .tmHolder = colors::tapMatchHolder,
    .tmSlot = colors::tapMatchSlot,
    .tmStatusText = rgb(235, 198, 208),

    .msField = rgb(74, 80, 92), // dark slate, like the original's field
    .msTileCovered = rgb(242, 244, 248),
    .msTileEdge = rgb(206, 210, 220),
    .msPanel = rgb(38, 42, 52),

    .bfField = rgb(38, 42, 52), // dark slate field, like the original
    .bfCell = rgb(74, 80, 92),  // mid-gray playable cell

    .snakeField = rgb(18, 20, 28),   // near-black navy void, like the original
    .snakeBorder = rgb(200, 72, 72), // red death edge

    .tfeBg = colors::cream,         // the classic 2048 off-white, matching the menu
    .tfeBoard = rgb(187, 173, 160), // the classic warm grey frame
    .tfeCell = rgb(205, 193, 180),  // the classic empty cell

    .memBgYou = rgb(196, 80, 88),        // rose-red table: your turn
    .memBgBot = rgb(46, 140, 170),       // teal table: the bot's turn
    .memPanel = rgb(52, 40, 48),         // dark plum scoreboard
    .memCardBack = rgb(40, 38, 52),      // the menu's card ink
    .memCardBackRing = rgb(92, 88, 118), // muted violet ring
    .memCardFace = rgb(252, 248, 240),   // warm white face

    .solTable = rgb(38, 122, 74),           // classic green felt
    .solSlot = rgb(255, 255, 255, 34),      // faint outline over the felt
    .solCardFace = rgb(253, 253, 250),      // white card
    .solCardBack = rgb(52, 84, 160),        // navy back
    .solCardBackInner = rgb(120, 146, 208), // lighter frame on the back
};

// Dark theme: charcoal chrome with near-white text; each game keeps its hue
// identity but darkened (Tic-Tac-Toe stays warm, Tap Match stays maroon).
inline constexpr Theme kDark{
    .appBg = rgb(18, 18, 20),
    .menuBg = rgb(20, 20, 24),
    .topBar = rgb(28, 28, 32),
    .barLine = rgb(48, 46, 56),
    .cardFill = rgb(46, 46, 52),
    .cardPressed = rgb(64, 64, 72),
    .primaryText = rgb(236, 236, 240),
    .titleText = rgb(240, 240, 245),
    .bodyText = rgb(236, 236, 240),
    .mutedText = rgb(150, 150, 160),
    .backCircle = rgb(44, 44, 50),
    .sliderTrack = rgb(58, 58, 66),

    .tttBg = rgb(58, 34, 30),          // deep burnt-umber: coral, much darker
    .tttPanel = rgb(40, 30, 30),       // darker warm brown panel
    .tttTab = rgb(56, 42, 42),         // difficulty tab
    .tttGridLine = rgb(228, 224, 220), // must invert: black lines vanish on dark

    .tmBg = rgb(54, 24, 34),           // deep wine: maroon, darker
    .tmTileLight = rgb(228, 226, 232), // stays light to back the emoji, off-white
    .tmTileDim = rgb(120, 116, 128),
    .tmTileEdge = rgb(70, 66, 78),
    .tmHolder = rgb(42, 18, 26),
    .tmSlot = rgb(30, 12, 18),
    .tmStatusText = rgb(220, 180, 192),

    .msField = rgb(30, 32, 38),          // darker slate
    .msTileCovered = rgb(220, 224, 232), // stays light to back the numbers/emoji
    .msTileEdge = rgb(150, 156, 170),
    .msPanel = rgb(22, 24, 30),

    .bfField = rgb(22, 24, 30), // darker slate field
    .bfCell = rgb(58, 64, 76),  // gray cell, dimmed for dark mode

    .snakeField = rgb(12, 13, 18),   // even darker void for dark mode
    .snakeBorder = rgb(168, 58, 58), // dimmer red edge

    .tfeBg = rgb(20, 20, 24),    // charcoal, like the dark menu
    .tfeBoard = rgb(62, 56, 52), // the warm frame, darkened
    .tfeCell = rgb(84, 76, 70),  // empty cell, darkened

    .memBgYou = rgb(74, 30, 36),    // deep wine table: your turn
    .memBgBot = rgb(20, 54, 66),    // deep teal table: the bot's turn
    .memPanel = rgb(34, 28, 32),    // darker plum scoreboard
    .memCardBack = rgb(70, 68, 88), // lifted so it reads on the dark table
    .memCardBackRing = rgb(112, 108, 140),
    .memCardFace = rgb(236, 232, 226), // stays light to back the emoji

    .solTable = rgb(22, 66, 42), // felt, darkened
    .solSlot = rgb(255, 255, 255, 28),
    .solCardFace = rgb(234, 234, 230), // stays light to back the ink
    .solCardBack = rgb(44, 64, 122),
    .solCardBackInner = rgb(96, 118, 176),
};

// The active theme, selected by Settings::darkMode. Scenes call this fresh every
// frame, so flipping dark mode recolors the whole app on the next frame.
[[nodiscard]] const Theme& theme();

} // namespace og
