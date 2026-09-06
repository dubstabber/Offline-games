#pragma once

#include "games/hexanaut/HexTypes.hpp"

#include <cstdint>
#include <span>

namespace og::hexanaut {

class HexWorld;

// A read-only window onto the simulation handed to bot controllers. This is the
// extension seam for AI: a bot reads only this view, so the engine never has to
// change for a new strategy. It is deliberately rich — per-cell ownership/trail
// and per-player state and speed — so a planner can do its own path search and
// threat scoring on top without new plumbing.
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
    // Seconds per hex for `id` right now — lets a bot compare who reaches a cell
    // first (the human is faster than the bots on every difficulty).
    [[nodiscard]] float stepIntervalOf(PlayerId id) const;

private:
    const HexWorld* world_;
};

// How boldly a difficulty's bots play. All three are the same planning bot with
// a different personality preset (see profileFor in HexBots.hpp): Cautious makes
// small loops and rarely chases, Ruthless plans big loops and hunts anyone whose
// trail it can reach in time.
enum class BotSkill : std::uint8_t { Cautious, Smart, Ruthless };

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
