#pragma once

#include "games/hexanaut/HexTypes.hpp"

#include <cstdint>
#include <span>

namespace og::hexanaut {

class HexWorld;

// A read-only window onto the simulation handed to bot controllers. This is the
// extension seam for AI: a smarter bot is just a new BotController subclass that
// reads only this view, so the engine never has to change. It is deliberately
// rich — per-cell ownership/trail, per-player state, and "go home" helpers — so a
// future planner (A* home, threat scoring, cut-off attacks) needs no new plumbing.
class HexWorldView {
public:
    explicit HexWorldView(const HexWorld& world) : world_(&world) {}

    [[nodiscard]] int gridW() const;
    [[nodiscard]] int gridH() const;
    [[nodiscard]] bool inBounds(HexCoord c) const;
    [[nodiscard]] PlayerId ownerAt(HexCoord c) const;
    [[nodiscard]] PlayerId trailOwnerAt(HexCoord c) const;
    [[nodiscard]] std::uint8_t powerupAt(HexCoord c) const;

    [[nodiscard]] int playerCount() const;
    [[nodiscard]] bool alive(PlayerId id) const;
    [[nodiscard]] HexCoord cellOf(PlayerId id) const;
    [[nodiscard]] HexCoord homeOf(PlayerId id) const;
    [[nodiscard]] HexDir headingOf(PlayerId id) const;
    [[nodiscard]] int territoryCount(PlayerId id) const;
    [[nodiscard]] std::span<const HexCoord> trailOf(PlayerId id) const;

    // Shortest hex-step distance from `from` to the nearest cell owned by `id`,
    // via a BFS bounded to `maxRadius` (returns maxRadius+1 if none within range).
    // The "return home" primitive a strong bot builds on.
    [[nodiscard]] int distanceToOwn(HexCoord from, PlayerId id, int maxRadius = 24) const;

private:
    const HexWorld* world_;
};

// Which controller a difficulty hands its bots. Smart arrives in a later phase as
// a new BotController subclass; the engine just maps the enum in makeBot().
enum class BotSkill : std::uint8_t { Basic, Smart };

// Abstract bot strategy. Implementations return the direction the bot wants to
// travel next; HexWorld applies it at the next cell centre (180° reversals and
// walls are filtered/deflected by the simulation, same as for the human).
struct BotController {
    BotController() = default;
    BotController(const BotController&) = delete;
    BotController& operator=(const BotController&) = delete;
    BotController(BotController&&) = delete;
    BotController& operator=(BotController&&) = delete;
    virtual ~BotController() = default;

    virtual HexDir decide(const HexWorldView& view, PlayerId self) = 0;
};

} // namespace og::hexanaut
