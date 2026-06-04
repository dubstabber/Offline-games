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

// One participant in the match. Movement is discrete hex-stepping: the player
// sits on `cell`, came from `fromCell`, and `stepProgress` in [0,1) is how far it
// has glided toward the next cell — the Scene uses it to interpolate a smooth
// avatar position. `trail` is the ordered run of cells claimed outside own
// territory since leaving it; closing the loop captures them.
struct Player {
    PlayerId id = 0;
    bool isBot = true;
    bool alive = true;

    HexCoord cell{};
    HexCoord fromCell{};
    HexCoord home{}; // spawn anchor inside own territory; bots steer back to it
    HexDir heading = HexDir::N;
    HexDir desiredDir = HexDir::None;

    float stepInterval = 0.15F;     // seconds per hex (lowered while Speed is active)
    float baseStepInterval = 0.15F; // restored when Speed wears off
    float stepProgress = 0.0F;      // [0,1) toward the next cell
    float speedTimer = 0.0F;        // seconds of Speed power-up left
    float visionTimer = 0.0F;       // seconds of Vision left (human only; the Scene reads it)

    int territoryCount = 0;
    int kills = 0;
    std::vector<HexCoord> trail;
    std::string name;
};

// The pure, SDL-free Hexanaut simulation: a bounded flat-top hex board plus the
// players that conquer it. Rules (movement, trail capture via flood fill,
// trail-cut / self-trail death, scoring) live here and are unit-tested; the Scene
// only renders this and feeds the human's desired direction. Determinism comes
// from a single seeded RNG and id-ordered resolution, so (difficulty, seed,
// inputs) replays identically.
class HexWorld {
public:
    HexWorld(int difficultyIndex, std::uint32_t seed);

    // The human is player 0. Sets the direction it will turn to at the next cell
    // center (ignored if it would be a 180° reversal).
    void setPlayerDesiredDir(HexDir dir);

    // Advance one fixed sub-step (config::kFixedDt). Accrues per-player step
    // progress and resolves any hex moves that cross a cell boundary.
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
    void killPlayer(PlayerId id);
    void respawnBot(PlayerId id);
    [[nodiscard]] HexCoord findSpawn(int clearRadius);
    void decideBots();
    void decayEffects();
    void maybeSpawnPowerup();
    static void applyPowerup(Player& p, PowerUp type);
    [[nodiscard]] HexDir deflectHeading(HexCoord cell, HexDir heading) const;

    // One tick of motion, split into compute-then-apply stages (mirrors how
    // SnakeWorld::step delegates to small sub-steps).
    struct Move {
        bool stepping = false;
        HexCoord target{};
        HexDir heading = HexDir::None;
    };
    [[nodiscard]] std::vector<Move> computeMoves();
    void detectDeaths(const std::vector<Move>& moves, std::vector<char>& dead);
    void commitMoves(const std::vector<Move>& moves, const std::vector<char>& dead);
    void respawnDeadBots();
    void resolveTick(bool allowRespawn);

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
