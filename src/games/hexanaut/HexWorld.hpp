#pragma once

#include "games/hexanaut/BotController.hpp"
#include "games/hexanaut/HexGrid.hpp"
#include "games/hexanaut/HexTypes.hpp"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace og::hexanaut {

// One participant in the match. Movement is free and continuous: the player has
// a world-space `pos` and a heading `angle` that curves toward `desiredAngle` at
// a capped turn rate, and `cell` is just the hex it currently sits over (updated
// when `pos` rounds into a new one). `heading`/`desiredDir` are the angle snapped
// to the 6 hex axes, kept so bots can still reason in discrete directions. `trail`
// is the ordered run of cells claimed outside own territory since leaving it;
// closing the loop captures them.
struct Player {
    PlayerId id = 0;
    bool isBot = true;
    bool alive = true;

    HexCoord cell{};
    HexCoord fromCell{};
    HexCoord home{};                  // spawn anchor inside own territory; bots steer back to it
    Vec2 pos;                         // continuous world position (the avatar's true location)
    float angle = 0.0F;               // current heading, radians
    float desiredAngle = 0.0F;        // heading the player steers toward
    HexDir heading = HexDir::N;       // `angle` snapped to 6 axes (for bots/view)
    HexDir desiredDir = HexDir::None; // bot/test desired axis -> desiredAngle

    float stepInterval = 0.15F;     // seconds per hex (lowered while Speed is active)
    float baseStepInterval = 0.15F; // restored when Speed wears off
    float speedTimer = 0.0F;        // seconds of Speed power-up left
    float visionTimer = 0.0F;       // seconds of Vision left (human only; the Scene reads it)

    int territoryCount = 0;
    int kills = 0;
    std::vector<HexCoord> trail;
    std::string name;
};

// The pure, SDL-free Hexanaut simulation: a bounded flat-top hex board plus the
// players that conquer it. Rules (free movement, trail capture via flood fill,
// trail-cut / self-trail death, scoring) live here and are unit-tested; the Scene
// only renders this and feeds the human's steering angle. Determinism comes from
// a single seeded RNG and id-ordered resolution, so (difficulty, seed, inputs)
// replays identically.
class HexWorld {
public:
    HexWorld(int difficultyIndex, std::uint32_t seed);

    // The human is player 0. Sets the world-space heading it steers toward; the
    // avatar curves to it at a capped turn rate (it cannot reverse instantly).
    void setPlayerDesiredAngle(float angleRad);

    // Advance one fixed sub-step (config::kFixedDt): turn each avatar toward its
    // desired heading, glide it forward, and resolve any hex it crosses into.
    void step();

    [[nodiscard]] const Player& player() const { return players_.front(); }
    [[nodiscard]] const std::vector<Player>& players() const { return players_; }
    [[nodiscard]] const HexGrid& grid() const { return grid_; }
    [[nodiscard]] bool playerAlive() const { return players_.front().alive; }
    [[nodiscard]] int totalCells() const { return totalCells_; }
    [[nodiscard]] int territoryCount(PlayerId id) const {
        return players_.at(static_cast<std::size_t>(id)).territoryCount;
    }
    [[nodiscard]] float percent(PlayerId id) const {
        return totalCells_ > 0 ? (100.0F * static_cast<float>(territoryCount(id)) /
                                  static_cast<float>(totalCells_))
                               : 0.0F;
    }
    [[nodiscard]] float playerPercent() const { return percent(0); }

    // ---- Test seams (SDL-free, used by tests/test_hexanaut.cpp) --------------
    [[nodiscard]] PlayerId ownerAt(HexCoord c) const { return grid_.at(c).owner; }
    [[nodiscard]] PlayerId trailOwnerAt(HexCoord c) const { return grid_.at(c).trailOwner; }
    [[nodiscard]] const std::vector<HexCoord>& trailOf(PlayerId id) const {
        return players_.at(static_cast<std::size_t>(id)).trail;
    }
    void setOwnerForTest(HexCoord c, PlayerId id) { setOwner(c, id); }
    void setPowerupForTest(HexCoord c, PowerUp type) {
        grid_.at(c).powerup = static_cast<std::uint8_t>(type);
    }
    void placePlayerForTest(PlayerId id, HexCoord cell, HexDir heading);
    void setDesiredDirForTest(PlayerId id, HexDir dir) {
        players_.at(static_cast<std::size_t>(id)).desiredDir = dir;
    }
    void setAliveForTest(PlayerId id, bool alive) {
        players_.at(static_cast<std::size_t>(id)).alive = alive;
    }
    // Force exactly one hex move for every alive player using its current
    // heading/desiredDir, running the real collision + capture path. Does NOT
    // respawn dead bots, so controlled collision scenarios stay set up.
    void advanceCellForTest();

private:
    void spawnHome(Player& p, HexCoord center, int radius);
    void setOwner(HexCoord c, PlayerId newOwner);
    void closeTrailAndCapture(Player& p);
    // `killer` is the player whose cut/collision felled `id`, or kNeutral when
    // there is no surviving aggressor (self-cut, head-to-head). A real killer
    // inherits the victim's territory; otherwise it is freed to neutral.
    void killPlayer(PlayerId id, PlayerId killer);
    void respawnBot(PlayerId id);
    [[nodiscard]] HexCoord findSpawn(int clearRadius);
    void decideBots();
    void decayEffects();
    void maybeSpawnPowerup();
    static void applyPowerup(Player& p, PowerUp type);
    [[nodiscard]] HexDir deflectHeading(HexCoord cell, HexDir heading) const;
    // Steer `angle` to the nearest heading whose `probe`-long forward step stays
    // on the board, so a free-moving avatar slides along walls instead of leaving.
    [[nodiscard]] float deflectAngle(Vec2 pos, float angle, float probe) const;

    // One tick of motion, split into compute-then-apply stages (mirrors how
    // SnakeWorld::step delegates to small sub-steps). `pos`/`angle`/`heading` are
    // the new pose; `entered` flags that the avatar crossed into `target` this
    // tick, which is what drives trail laying, capture, and collisions.
    struct Move {
        bool entered = false;
        HexCoord target{};
        HexDir heading = HexDir::None;
        float angle = 0.0F;
        Vec2 pos;
    };
    [[nodiscard]] std::vector<Move> integrateMotion(); // production: continuous glide
    [[nodiscard]] std::vector<Move> forcedCellMoves(); // tests: force one hex step
    // Fills `dead` with who fell this tick and `killer` with the player that
    // felled each of them (kNeutral = none/mutual), so a cutter can inherit land.
    void detectDeaths(const std::vector<Move>& moves, std::vector<char>& dead,
                      std::vector<PlayerId>& killer);
    void commitMoves(const std::vector<Move>& moves, const std::vector<char>& dead);
    void respawnDeadBots();
    void resolveTick(const std::vector<Move>& moves, bool allowRespawn);

    HexGrid grid_;
    std::vector<Player> players_;
    std::vector<std::unique_ptr<BotController>> bots_; // parallel to players_; null for the human
    std::mt19937 rng_;
    int totalCells_;
    std::vector<std::uint8_t> visited_; // flood-fill scratch, sized to the grid

    float powerupAccum_ = 0.0F;
    float powerupInterval_ = 0.0F;
    int maxPowerups_ = 0;
    int activePowerups_ = 0;
};

} // namespace og::hexanaut
