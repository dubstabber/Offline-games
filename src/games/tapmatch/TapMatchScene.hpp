#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/tapmatch/TapMatchBoard.hpp"
#include "ui/Button.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace og {

class SceneManager;

// Renders a Tap Match board — overlapping piles of fruit-emoji tiles on a maroon
// field with a 7-slot holder bar — and turns taps into moves. Tapping an
// uncovered (bright) tile sends its fruit to the holder; three of a kind clear.
// Clear the board to win; fill the holder with no triple to lose. Difficulty
// scales fruit variety, tile count, and how deep the piles stack.
class TapMatchScene : public Scene {
public:
    TapMatchScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    // Playing: accept taps. GameOver: the result overlay is shown and only its
    // buttons respond.
    enum class Phase : std::uint8_t { Playing, GameOver };

    bool handleBackButton(const PointerEvent& event);
    void beginRound();
    void enterGameOver();

    // The id of the uncovered tile under (px, py), or -1 if the topmost tile
    // there is covered or there is none.
    [[nodiscard]] int pickAccessibleAt(float px, float py) const;
    [[nodiscard]] std::string statusText() const;
    [[nodiscard]] const char* resultText() const;

    static void drawBackButton(Canvas& canvas);
    void drawTopBar(Canvas& canvas) const;
    void drawBoard(Canvas& canvas) const;
    static void drawHolder(Canvas& canvas);
    void drawTray(Canvas& canvas) const;
    void drawFlying(Canvas& canvas) const;
    void drawSparks(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    // ---- Animation/VFX (a visual layer over the pure, instant board) --------
    // A fruit in the holder. The active (non-pending) tiles mirror board_.holder();
    // a completing triple's three tiles stay in the tray as `pending` (no longer
    // grouped) so the holder briefly shows the finished triple, then they pop out.
    struct TrayTile {
        int uid = 0;
        int icon = 0;
        float x = 0.0F;        // animated slot-centre x (slides on reflow)
        bool arriving = false; // true while a FlyTile is still delivering it
        bool pending = false;  // part of a completing triple: logically gone
        float clear = -1.0F;   // <0 = not popping; else pop age (hold, then shrink)
    };
    // A fruit mid-flight from its board tile to the holder slot of its tray tile.
    struct FlyTile {
        int uid = 0; // the tray tile it delivers
        int icon = 0;
        float x0 = 0.0F; // start centre (the board tile)
        float y0 = 0.0F;
        float size0 = 0.0F;
        float t = 0.0F;
        float dur = 0.0F;
    };
    // A spark in the match burst.
    struct Spark {
        float x = 0.0F;
        float y = 0.0F;
        float vx = 0.0F;
        float vy = 0.0F;
        float age = 0.0F;
        float life = 0.0F;
    };

    void resetFx(); // clear all animation state, restart the intro
    [[nodiscard]] bool introActive() const;
    void startTap(int id);                 // tap the board tile and spawn its animations
    void advanceFx(float dtSeconds);       // advance flights, reflow, pops, sparks
    void maybeStartClear(int icon);        // pop a triple once all three have landed
    [[nodiscard]] bool anyPending() const; // a match is still animating out
    [[nodiscard]] int trayIndexOf(int uid) const;
    void spawnSparks(float x, float y);

    SceneManager& manager_;
    Difficulty difficulty_;
    TapMatchBoard board_;
    Phase phase_ = Phase::Playing;
    bool backPressed_ = false;
    Button homeButton_;
    Button playAgainButton_;

    std::vector<TrayTile> tray_;  // holder fruit (settled, arriving, and popping)
    std::vector<FlyTile> flying_; // fruit in flight to the holder
    std::vector<Spark> sparks_;   // match-burst particles
    float introClock_ = 0.0F;     // time since the round started (board fly-in)
    float introEnd_ = 0.0F;       // when the intro finishes (taps unlock)
    int nextUid_ = 1;
    bool wonPending_ = false;  // board is won; show the overlay once anims settle
    bool losePending_ = false; // board is lost; let the last fruit land first
};

} // namespace og
