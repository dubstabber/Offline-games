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

    float stepInterval = 0.15F;     // seconds per hex
    float baseStepInterval = 0.15F; // restored on respawn
    float teleportCooldown = 0.0F;  // seconds until another paired teleport may trigger

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
// A persistent map shooter. It sits on a fixed `cell`; while that cell lies in
// some player's territory the shooter captures the nearest un-owned/enemy cell
// for that owner, one at a time, at a distance-scaled rate. Each capture bumps
// `shotCount` and records `target`, so the Scene can fire a single fading laser
// per shot (the beam is a transient muzzle flash, not a continuous ray). The
// shooter is owned by whoever owns `cell`, so recapturing the cell steals it.
struct Shooter {
    HexCoord cell{};
    HexCoord target{};           // the most recent cell it fired at
    float cooldown = 0.0F;       // seconds until the next capture
    std::uint32_t shotCount = 0; // bumped on each capture -> one laser bolt per bump
};

// A persistent slowing totem. It sits on a fixed `cell`; while that cell is owned
// by a player it blankets a hex radius (config::kSlowRadius) in snow and slows
// every OTHER avatar standing in that field — the owner is immune. Owned by
// whoever owns `cell`, so recapturing the cell steals it. Does nothing while the
// cell is neutral.
struct SlowTotem {
    HexCoord cell{};
};

// A persistent spy dish. While its cell is owned by the human, every territory is
// revealed on the minimap (otherwise rivals' territories and position markers are
// hidden). A pure intel item — owning more than one is no better than one. Owned
// by whoever owns `cell`, so it can be stolen back by recapturing the cell.
struct SpyDish {
    HexCoord cell{};
};

// A persistent bidirectional teleport relationship. The endpoints are fixed map
// items; unrelated endpoints never connect, even if a player owns several.
struct TeleportPair {
    HexCoord a{};
    HexCoord b{};
};

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
    [[nodiscard]] const std::vector<Shooter>& shooters() const { return shooters_; }
    [[nodiscard]] const std::vector<SlowTotem>& slowTotems() const { return slowTotems_; }
    [[nodiscard]] const std::vector<SpyDish>& spyDishes() const { return spyDishes_; }
    [[nodiscard]] const std::vector<TeleportPair>& teleports() const { return teleports_; }
    // True if `id` owns at least one spy dish (so the minimap reveals all
    // territories for them). Owning several is no different from owning one.
    [[nodiscard]] bool hasSpyReveal(PlayerId id) const;
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
    // Drop a shooter on `c` (and register it) so the laser-capture mechanic can be
    // driven in isolation; advanceShootersForTest runs N shooter ticks only.
    void setShooterForTest(HexCoord c) {
        grid_.at(c).powerup = static_cast<std::uint8_t>(PowerUp::Shooter);
        shooters_.push_back(Shooter{.cell = c});
    }
    void setSlowTotemForTest(HexCoord c) {
        grid_.at(c).powerup = static_cast<std::uint8_t>(PowerUp::SlowTotem);
        slowTotems_.push_back(SlowTotem{.cell = c});
    }
    void setSpyDishForTest(HexCoord c) {
        grid_.at(c).powerup = static_cast<std::uint8_t>(PowerUp::SpyDish);
        spyDishes_.push_back(SpyDish{.cell = c});
    }
    void setTeleportPairForTest(HexCoord a, HexCoord b) {
        grid_.at(a).powerup = static_cast<std::uint8_t>(PowerUp::Teleport);
        grid_.at(b).powerup = static_cast<std::uint8_t>(PowerUp::Teleport);
        teleports_.push_back(TeleportPair{.a = a, .b = b});
    }
    void advanceShootersForTest(int ticks) {
        for (int i = 0; i < ticks; ++i) {
            updateShooters();
        }
    }

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
    // Place `count` static items once at construction: map prizes that never move
    // or respawn, shown on the minimap and contested via territory.
    void generateShooters(int count);
    void generateSlowTotems(int count);
    void generateSpyDishes(int count);
    void generateTeleports(int pairCount);
    // A random neutral, trail-free, item-free cell to drop a static item on; false
    // if none found within the attempt budget. Shared by the generators above.
    [[nodiscard]] bool findFreeItemCell(HexCoord& out);
    [[nodiscard]] bool findFreeTeleportPair(HexCoord& a, HexCoord& b);
    // True if `cell` lies in the slowing field of a totem owned by someone other
    // than `id` (the owner is immune), so that avatar moves slower this tick.
    [[nodiscard]] bool inEnemySlowField(PlayerId id, HexCoord cell) const;
    // Advance every shooter one tick: capture the nearest in-range cell for its
    // current owner at a distance-scaled rate, and refresh its firing/target so
    // the Scene can draw the beam. Owner-neutral shooters idle.
    void updateShooters();
    // Nearest in-bounds cell not owned by `owner`, within kShooterRange of `from`,
    // via bounded BFS. Returns false if none in range. `from` must be owner-owned.
    [[nodiscard]] bool nearestShooterTarget(HexCoord from, PlayerId owner, HexCoord& out,
                                            int& outDist) const;
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
    // Rewrite a move that enters an active owned teleport endpoint so the usual
    // death/capture pass resolves at the paired destination.
    void resolveTeleports(std::vector<Move>& moves);
    [[nodiscard]] bool pairedTeleportDestination(PlayerId id, HexCoord from, HexCoord& out) const;
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
    std::vector<Shooter> shooters_;                    // persistent laser items on the board
    std::vector<SlowTotem> slowTotems_;                // persistent slowing-field items
    std::vector<SpyDish> spyDishes_;                   // persistent minimap-reveal items
    std::vector<TeleportPair> teleports_;              // persistent paired teleport endpoints
    std::mt19937 rng_;
    int totalCells_;
    std::vector<std::uint8_t> visited_; // flood-fill scratch, sized to the grid
};

} // namespace og::hexanaut
