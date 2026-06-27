#pragma once

#include "games/nibbles/NibblesTypes.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace og::nibbles {

enum class BonusType : std::uint8_t { Regular, Half, Double, Life, Reverse };
enum class NibblesStatus : std::uint8_t { Playing, LevelComplete, GameOver };

struct NibblesConfig {
    int wormCount = 4;
    int tickMs = 105;
    bool fakes = false;
    int regularBonusCount = 0; // 0 = original-style 8 + wormCount
};

struct Bonus {
    BonusType type = BonusType::Regular;
    Position pos; // top-left of the 2x2 bonus footprint
    bool fake = false;
    int countdown = 0;
};

struct Worm {
    int id = 0;
    bool isHuman = false;
    int lives = 6;
    int score = 0;
    int change = 0;
    int dematerialized = 0;
    int still = 0;
    Direction direction = Direction::Right;
    Direction startDirection = Direction::Right;
    Position start;
    std::vector<Position> body; // head first
    std::vector<Direction> queuedTurns;

    [[nodiscard]] bool alive() const { return lives > 0 && !body.empty(); }
    [[nodiscard]] bool materialized() const { return dematerialized <= 0; }
    [[nodiscard]] Position head() const { return body.empty() ? start : body.front(); }
    [[nodiscard]] int length() const { return static_cast<int>(body.size()); }
};

class NibblesWorld {
public:
    NibblesWorld(NibblesLevel level, NibblesConfig config, std::uint32_t seed);

    void queueTurn(Direction direction);
    void step();

    [[nodiscard]] const NibblesLevel& level() const { return level_; }
    [[nodiscard]] const NibblesConfig& config() const { return config_; }
    [[nodiscard]] const std::vector<Worm>& worms() const { return worms_; }
    [[nodiscard]] const std::vector<Bonus>& bonuses() const { return bonuses_; }
    [[nodiscard]] NibblesStatus status() const { return status_; }
    [[nodiscard]] int regularLeft() const { return regularLeft_; }
    [[nodiscard]] int regularTotal() const { return regularTotal_; }
    [[nodiscard]] int score() const { return worms_.empty() ? 0 : worms_.front().score; }
    [[nodiscard]] int lives() const { return worms_.empty() ? 0 : worms_.front().lives; }
    [[nodiscard]] float tickSeconds() const;
    [[nodiscard]] Cell cellAt(int x, int y) const;
    [[nodiscard]] Position wrapped(Position pos) const;

    // Test seams for precise, SDL-free checks.
    [[nodiscard]] Worm& wormRef(std::size_t index) { return worms_.at(index); }
    void clearBonuses() { bonuses_.clear(); }
    void addBonus(const Bonus& bonus) { bonuses_.push_back(bonus); }
    void setRegularLeft(int count) { regularLeft_ = std::max(0, count); }
    void setCell(int x, int y, Cell cell);

private:
    struct PlannedMove {
        bool moving = false;
        bool warpBonus = false;
        Position next;
    };

    void resetLevel();
    void initWorms();
    void spawnWorm(Worm& worm, bool loseLife) const;
    void decayStillTimers();
    void applyMissedPenalty();
    void applyTurns();
    void applyHumanTurn(Worm& worm);
    void applyAiTurn(Worm& worm);
    [[nodiscard]] Direction chooseAiDirection(const Worm& worm);
    [[nodiscard]] int aiDirectionCost(const Worm& worm, Direction direction);
    void ageBonuses();
    void maybeSpawnSpecialBonus();
    void planMoves(std::vector<PlannedMove>& moves, std::vector<bool>& dead);
    void markHeadCollisions(const std::vector<PlannedMove>& moves, std::vector<bool>& dead) const;
    void applyMoves(const std::vector<PlannedMove>& moves, const std::vector<bool>& dead);
    void applyDeaths(const std::vector<bool>& dead);
    [[nodiscard]] PlannedMove planMove(const Worm& worm);
    [[nodiscard]] bool canMoveTo(const Worm& worm, Position pos) const;
    [[nodiscard]] bool materializedWormAt(Position pos) const;
    [[nodiscard]] const Warp* warpAt(Position pos) const;
    [[nodiscard]] Position warpExit(const Worm& worm, Position entry, const Warp& warp,
                                    bool& bonus);
    [[nodiscard]] Position randomWarpExit(const Worm& worm);
    static void moveWorm(Worm& worm, Position next);
    void killWorm(Worm& worm);
    void applyBonusAt(Worm& worm);
    void applyBonus(Worm& worm, const Bonus& bonus);
    void reverseWorm(Worm& worm) const;
    static void reduceTail(Worm& worm, int cells);
    void addBonus(bool regular);
    void addOneBonus(BonusType type, bool fake, int countdown);
    [[nodiscard]] bool findBonusSpace(Position& out);
    [[nodiscard]] bool bonusOverlaps(Position pos) const;
    [[nodiscard]] bool bonusAt(Position pos, std::size_t& index) const;
    [[nodiscard]] bool twoByTwoEmpty(Position pos) const;
    [[nodiscard]] int nearestBonusDistance(Position from) const;
    [[nodiscard]] int manhattanWrapped(Position a, Position b) const;
    [[nodiscard]] int irand(int lo, int hi);

    NibblesLevel level_;
    NibblesConfig config_;
    std::mt19937 rng_;
    std::vector<Worm> worms_;
    std::vector<Bonus> bonuses_;
    NibblesStatus status_ = NibblesStatus::Playing;
    int regularTotal_ = 0;
    int regularLeft_ = 0;
    int missedRegular_ = 0;
    int tick_ = 0;
};

[[nodiscard]] NibblesConfig nibblesConfigForDifficulty(int difficultyIndex);

} // namespace og::nibbles
