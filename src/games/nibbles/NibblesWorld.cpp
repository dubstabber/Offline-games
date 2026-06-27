#include "games/nibbles/NibblesWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace og::nibbles {
namespace {

constexpr int kStartingLength = 5;
constexpr int kStartingLives = 6;
constexpr int kMaxLives = 12;
constexpr int kGrowFactor = 4;
constexpr int kRegularCountdown = 300;
constexpr int kHalfCountdown = 200;
constexpr int kDoubleCountdown = 150;
constexpr int kLifeCountdown = 100;
constexpr int kReverseCountdown = 150;
constexpr int kMaxQueuedTurns = 8;
constexpr int kMissedLimit = 2;
constexpr int kRandomWarpProbeCount = 200;

[[nodiscard]] Position cellCenter(const Bonus& bonus) {
    return {.x = bonus.pos.x + 1, .y = bonus.pos.y + 1};
}

[[nodiscard]] NibblesConfig normalizedConfig(NibblesConfig config) {
    config.wormCount = std::clamp(config.wormCount, 1, 6);
    config.tickMs = std::clamp(config.tickMs, 35, 300);
    return config;
}

[[nodiscard]] int regularTotalFor(const NibblesConfig& config) {
    return config.regularBonusCount > 0 ? config.regularBonusCount : 8 + config.wormCount;
}

[[nodiscard]] std::size_t cellIndex(int x, int y, int width) {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) +
           static_cast<std::size_t>(x);
}

[[nodiscard]] Direction directionBetween(Position from, Position to, int width, int height) {
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    if (dx == 1 || dx < -(width / 2)) {
        return Direction::Right;
    }
    if (dx == -1 || dx > width / 2) {
        return Direction::Left;
    }
    if (dy == 1 || dy < -(height / 2)) {
        return Direction::Down;
    }
    return Direction::Up;
}

} // namespace

NibblesConfig nibblesConfigForDifficulty(int difficultyIndex) {
    switch (std::clamp(difficultyIndex, 0, 2)) {
    case 0:
        return {.wormCount = 4, .tickMs = 140, .fakes = false, .regularBonusCount = 0};
    case 2:
        return {.wormCount = 6, .tickMs = 70, .fakes = true, .regularBonusCount = 0};
    default:
        break;
    }
    return {.wormCount = 4, .tickMs = 105, .fakes = false, .regularBonusCount = 0};
}

NibblesWorld::NibblesWorld(NibblesLevel level, NibblesConfig config, std::uint32_t seed)
    : level_(std::move(level)), config_(normalizedConfig(config)), rng_(seed),
      regularTotal_(regularTotalFor(config_)) {
    resetLevel();
}

float NibblesWorld::tickSeconds() const {
    return static_cast<float>(config_.tickMs) / 1000.0F;
}

Cell NibblesWorld::cellAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= level_.width || y >= level_.height || level_.cells.empty()) {
        return Cell::Wall;
    }
    return level_.cells.at(cellIndex(x, y, level_.width));
}

void NibblesWorld::setCell(int x, int y, Cell cell) {
    if (x < 0 || y < 0 || x >= level_.width || y >= level_.height || level_.cells.empty()) {
        return;
    }
    level_.cells.at(cellIndex(x, y, level_.width)) = cell;
}

Position NibblesWorld::wrapped(Position pos) const {
    if (level_.width <= 0 || level_.height <= 0) {
        return pos;
    }
    if (pos.x < 0) {
        pos.x = level_.width - 1;
    } else if (pos.x >= level_.width) {
        pos.x = 0;
    }
    if (pos.y < 0) {
        pos.y = level_.height - 1;
    } else if (pos.y >= level_.height) {
        pos.y = 0;
    }
    return pos;
}

void NibblesWorld::resetLevel() {
    bonuses_.clear();
    regularLeft_ = regularTotal_;
    missedRegular_ = 0;
    tick_ = 0;
    status_ = NibblesStatus::Playing;
    initWorms();
    addBonus(true);
}

void NibblesWorld::initWorms() {
    worms_.clear();
    worms_.resize(static_cast<std::size_t>(config_.wormCount));
    for (std::size_t i = 0; i < worms_.size(); ++i) {
        Worm& worm = worms_.at(i);
        worm.id = static_cast<int>(i);
        worm.isHuman = i == 0;
        worm.lives = kStartingLives;
        worm.score = 0;
        const Spawn spawn = i < level_.spawns.size()
                                ? level_.spawns.at(i)
                                : Spawn{.pos = {.x = 2 + (static_cast<int>(i) * 3),
                                                .y = 2 + (static_cast<int>(i) * 2)},
                                        .direction = Direction::Right};
        worm.start = wrapped(spawn.pos);
        worm.startDirection = spawn.direction;
        spawnWorm(worm, false);
    }
}

void NibblesWorld::spawnWorm(Worm& worm, bool loseLife) const {
    if (loseLife && worm.lives > 0) {
        --worm.lives;
    }
    worm.body.clear();
    worm.queuedTurns.clear();
    worm.change = 0;
    worm.still = 0;
    worm.dematerialized = 0;
    worm.direction = worm.startDirection;
    if (worm.lives <= 0) {
        return;
    }

    Position pos = worm.start;
    worm.body.push_back(pos);
    for (int i = 1; i < kStartingLength; ++i) {
        pos = wrapped(add(pos, delta(opposite(worm.startDirection))));
        worm.body.push_back(pos);
    }
    worm.dematerialized = kStartingLength;
}

void NibblesWorld::queueTurn(Direction direction) {
    if (worms_.empty()) {
        return;
    }
    Worm& player = worms_.front();
    if (!player.queuedTurns.empty() && player.queuedTurns.back() == direction) {
        return;
    }
    if (static_cast<int>(player.queuedTurns.size()) >= kMaxQueuedTurns) {
        return;
    }
    player.queuedTurns.push_back(direction);
}

void NibblesWorld::step() {
    if (status_ != NibblesStatus::Playing || worms_.empty()) {
        return;
    }

    ++tick_;
    decayStillTimers();
    applyMissedPenalty();
    ageBonuses();
    maybeSpawnSpecialBonus();
    applyTurns();

    std::vector<PlannedMove> moves(worms_.size());
    std::vector<bool> dead(worms_.size(), false);
    planMoves(moves, dead);
    markHeadCollisions(moves, dead);
    applyMoves(moves, dead);
    applyDeaths(dead);
    if (worms_.front().lives <= 0) {
        status_ = NibblesStatus::GameOver;
    }
}

void NibblesWorld::decayStillTimers() {
    for (Worm& worm : worms_) {
        if (worm.still > 0) {
            --worm.still;
        }
    }
}

void NibblesWorld::applyMissedPenalty() {
    if (missedRegular_ > kMissedLimit && worms_.front().score > 0) {
        --worms_.front().score;
    }
}

void NibblesWorld::planMoves(std::vector<PlannedMove>& moves, std::vector<bool>& dead) {
    for (std::size_t i = 0; i < worms_.size(); ++i) {
        Worm& worm = worms_.at(i);
        if (!worm.alive() || worm.still > 0) {
            continue;
        }
        moves.at(i) = planMove(worm);
        if (!moves.at(i).moving || !canMoveTo(worm, moves.at(i).next)) {
            dead.at(i) = true;
        }
    }
}

void NibblesWorld::markHeadCollisions(const std::vector<PlannedMove>& moves,
                                      std::vector<bool>& dead) const {
    for (std::size_t i = 0; i < moves.size(); ++i) {
        if (dead.at(i) || !moves.at(i).moving || !worms_.at(i).materialized()) {
            continue;
        }
        for (std::size_t j = i + 1; j < moves.size(); ++j) {
            if (dead.at(j) || !moves.at(j).moving || !worms_.at(j).materialized()) {
                continue;
            }
            if (moves.at(i).next == moves.at(j).next) {
                dead.at(i) = true;
                dead.at(j) = true;
            }
        }
    }
}

void NibblesWorld::applyMoves(const std::vector<PlannedMove>& moves,
                              const std::vector<bool>& dead) {
    for (std::size_t i = 0; i < worms_.size(); ++i) {
        if (!dead.at(i) && moves.at(i).moving) {
            Worm& worm = worms_.at(i);
            moveWorm(worm, moves.at(i).next);
            if (moves.at(i).warpBonus) {
                worm.score += (worm.length() * std::max(1, level_.sourceLevel)) / 2;
            }
            applyBonusAt(worm);
        }
    }
}

void NibblesWorld::applyDeaths(const std::vector<bool>& dead) {
    for (std::size_t i = 0; i < worms_.size(); ++i) {
        if (dead.at(i)) {
            killWorm(worms_.at(i));
        }
    }
}

void NibblesWorld::applyTurns() {
    for (Worm& worm : worms_) {
        if (!worm.alive() || worm.still > 0) {
            continue;
        }
        if (worm.isHuman) {
            applyHumanTurn(worm);
        } else {
            applyAiTurn(worm);
        }
    }
}

void NibblesWorld::applyHumanTurn(Worm& worm) {
    while (!worm.queuedTurns.empty()) {
        const Direction next = worm.queuedTurns.front();
        worm.queuedTurns.erase(worm.queuedTurns.begin());
        if (next == opposite(worm.direction)) {
            continue;
        }
        const Direction old = worm.direction;
        worm.direction = next;
        if (canMoveTo(worm, planMove(worm).next)) {
            return;
        }
        worm.direction = old;
    }
}

void NibblesWorld::applyAiTurn(Worm& worm) {
    worm.direction = chooseAiDirection(worm);
}

Direction NibblesWorld::chooseAiDirection(const Worm& worm) {
    Direction best = worm.direction;
    int bestCost = std::numeric_limits<int>::max();
    const std::array<Direction, 4> candidates{worm.direction, turnLeft(worm.direction),
                                              turnRight(worm.direction), opposite(worm.direction)};
    for (Direction direction : candidates) {
        if (direction == opposite(worm.direction)) {
            continue;
        }
        const int cost = aiDirectionCost(worm, direction);
        if (cost < bestCost) {
            best = direction;
            bestCost = cost;
        }
    }
    if (bestCost == std::numeric_limits<int>::max()) {
        return worm.direction;
    }
    return best;
}

int NibblesWorld::aiDirectionCost(const Worm& worm, Direction direction) {
    Worm probe = worm;
    probe.direction = direction;
    const PlannedMove move = planMove(probe);
    if (!move.moving || !canMoveTo(worm, move.next)) {
        return std::numeric_limits<int>::max();
    }
    int cost = nearestBonusDistance(move.next) * 4;
    if (direction != worm.direction) {
        cost += 10;
    }
    for (const Worm& other : worms_) {
        if (other.id == worm.id || !other.alive()) {
            continue;
        }
        const int d = manhattanWrapped(move.next, other.head());
        if (d <= 3) {
            cost += (4 - d) * 80;
        }
    }
    cost += irand(0, 8);
    return cost;
}

void NibblesWorld::ageBonuses() {
    for (std::size_t i = bonuses_.size(); i > 0; --i) {
        Bonus& bonus = bonuses_.at(i - 1);
        if (bonus.countdown > 0) {
            --bonus.countdown;
            continue;
        }
        const bool missedRegular = bonus.type == BonusType::Regular && !bonus.fake;
        bonuses_.erase(bonuses_.begin() + static_cast<std::ptrdiff_t>(i - 1));
        if (missedRegular) {
            ++missedRegular_;
            addBonus(true);
        }
    }
}

void NibblesWorld::maybeSpawnSpecialBonus() {
    if (tick_ % 3 == 0) {
        addBonus(false);
    }
}

NibblesWorld::PlannedMove NibblesWorld::planMove(const Worm& worm) {
    if (!worm.alive()) {
        return {};
    }
    PlannedMove move;
    move.moving = true;
    move.next = wrapped(add(worm.head(), delta(worm.direction)));
    if (const Warp* warp = warpAt(move.next); warp != nullptr) {
        move.next = warpExit(worm, move.next, *warp, move.warpBonus);
    }
    return move;
}

bool NibblesWorld::canMoveTo(const Worm& worm, Position pos) const {
    const Cell cell = cellAt(pos.x, pos.y);
    if (cell == Cell::Wall || cell == Cell::Warp) {
        return false;
    }
    if (materializedWormAt(pos)) {
        return !worm.materialized();
    }
    return true;
}

bool NibblesWorld::materializedWormAt(Position pos) const {
    return std::ranges::any_of(worms_, [pos](const Worm& worm) {
        return worm.alive() && worm.materialized() &&
               std::ranges::find(worm.body, pos) != worm.body.end();
    });
}

const Warp* NibblesWorld::warpAt(Position pos) const {
    for (const Warp& warp : level_.warps) {
        const bool inside = pos.x >= warp.source.x && pos.x <= warp.source.x + 1 &&
                            pos.y >= warp.source.y && pos.y <= warp.source.y + 1;
        if (inside) {
            return &warp;
        }
    }
    return nullptr;
}

Position NibblesWorld::warpExit(const Worm& worm, Position entry, const Warp& warp, bool& bonus) {
    bonus = false;
    if (warp.random) {
        bonus = true;
        return randomWarpExit(worm);
    }
    if (!warp.bidirectional) {
        return wrapped(warp.target);
    }

    Position out = warp.target;
    if (worm.direction == Direction::Left || worm.direction == Direction::Right) {
        out.x = entry.x == warp.source.x ? warp.target.x + 2 : warp.target.x - 1;
        out.y = entry.y == warp.source.y ? warp.target.y : warp.target.y + 1;
    } else {
        out.x = entry.x == warp.source.x ? warp.target.x : warp.target.x + 1;
        out.y = entry.y == warp.source.y ? warp.target.y + 2 : warp.target.y - 1;
    }
    return wrapped(out);
}

Position NibblesWorld::randomWarpExit(const Worm& worm) {
    if (level_.width <= 0 || level_.height <= 0) {
        return worm.head();
    }
    std::uniform_int_distribution<int> xdist(0, level_.width - 1);
    std::uniform_int_distribution<int> ydist(0, level_.height - 1);
    for (int i = 0; i < kRandomWarpProbeCount; ++i) {
        const Position pos{.x = xdist(rng_), .y = ydist(rng_)};
        if (cellAt(pos.x, pos.y) == Cell::Empty && !materializedWormAt(pos)) {
            return pos;
        }
    }
    return worm.head();
}

void NibblesWorld::moveWorm(Worm& worm, Position next) {
    worm.body.insert(worm.body.begin(), next);
    if (worm.change > 0) {
        --worm.change;
    } else if (worm.body.size() > 1) {
        worm.body.pop_back();
    }
    if (worm.dematerialized > 0) {
        --worm.dematerialized;
    }
}

void NibblesWorld::killWorm(Worm& worm) {
    if (config_.wormCount > 1) {
        worm.score = worm.score * 7 / 10;
    }
    spawnWorm(worm, true);
    if (worm.alive()) {
        worm.still = 2;
    }
}

void NibblesWorld::applyBonusAt(Worm& worm) {
    std::size_t index = 0;
    if (!bonusAt(worm.head(), index)) {
        return;
    }
    const Bonus bonus = bonuses_.at(index);
    bonuses_.erase(bonuses_.begin() + static_cast<std::ptrdiff_t>(index));
    applyBonus(worm, bonus);
}

void NibblesWorld::applyBonus(Worm& worm, const Bonus& bonus) {
    if (bonus.fake) {
        reverseWorm(worm);
        return;
    }

    const int levelScore = std::max(1, level_.sourceLevel);
    switch (bonus.type) {
    case BonusType::Regular: {
        missedRegular_ = 0;
        const int nth = std::max(1, regularTotal_ - regularLeft_ + 1);
        worm.change += nth * kGrowFactor;
        worm.score += nth * levelScore;
        regularLeft_ = std::max(0, regularLeft_ - 1);
        if (regularLeft_ == 0) {
            status_ = NibblesStatus::LevelComplete;
        } else {
            addBonus(true);
        }
        break;
    }
    case BonusType::Half: {
        const int effectiveLength = worm.length() + worm.change;
        if (effectiveLength > 2) {
            worm.score += ((worm.length() + (worm.change / 2)) * levelScore);
            reduceTail(worm, worm.length() / 2);
            worm.change -= effectiveLength / 2;
        }
        break;
    }
    case BonusType::Double:
        worm.score += (worm.length() + worm.change) * levelScore;
        worm.change += worm.length() + worm.change;
        break;
    case BonusType::Life:
        worm.lives = std::min(kMaxLives, worm.lives + 1);
        break;
    case BonusType::Reverse:
        for (Worm& other : worms_) {
            if (other.id != worm.id) {
                reverseWorm(other);
            }
        }
        break;
    }
}

void NibblesWorld::reverseWorm(Worm& worm) const {
    if (!worm.alive() || worm.body.size() < 2) {
        return;
    }
    std::ranges::reverse(worm.body);
    worm.direction =
        directionBetween(worm.body.at(1), worm.body.front(), level_.width, level_.height);
    worm.queuedTurns.clear();
}

void NibblesWorld::reduceTail(Worm& worm, int cells) {
    for (int i = 0; i < cells && worm.body.size() > 2; ++i) {
        worm.body.pop_back();
    }
}

void NibblesWorld::addBonus(bool regular) {
    if (regular) {
        if (config_.fakes && irand(0, 6) == 0) {
            addOneBonus(BonusType::Regular, true, kRegularCountdown);
        }
        addOneBonus(BonusType::Regular, false, kRegularCountdown);
        return;
    }

    if (missedRegular_ > kMissedLimit || irand(0, 49) != 0) {
        return;
    }
    const bool fake = config_.fakes && irand(0, 6) == 0;
    const int roll = irand(0, 20);
    if (roll < 10) {
        addOneBonus(BonusType::Half, fake, kHalfCountdown);
    } else if (roll < 15) {
        addOneBonus(BonusType::Double, fake, kDoubleCountdown);
    } else if (roll == 15) {
        addOneBonus(BonusType::Life, fake, kLifeCountdown);
    } else if (config_.wormCount > 1) {
        addOneBonus(BonusType::Reverse, fake, kReverseCountdown);
    }
}

void NibblesWorld::addOneBonus(BonusType type, bool fake, int countdown) {
    Position pos;
    if (!findBonusSpace(pos)) {
        return;
    }
    bonuses_.push_back(Bonus{.type = type, .pos = pos, .fake = fake, .countdown = countdown});
}

bool NibblesWorld::findBonusSpace(Position& out) {
    if (level_.width < 2 || level_.height < 2) {
        return false;
    }
    for (int i = 0; i < 400; ++i) {
        const Position pos{.x = irand(0, level_.width - 2), .y = irand(0, level_.height - 2)};
        if (twoByTwoEmpty(pos)) {
            out = pos;
            return true;
        }
    }
    for (int y = 0; y < level_.height - 1; ++y) {
        for (int x = 0; x < level_.width - 1; ++x) {
            const Position pos{.x = x, .y = y};
            if (twoByTwoEmpty(pos)) {
                out = pos;
                return true;
            }
        }
    }
    return false;
}

bool NibblesWorld::bonusOverlaps(Position pos) const {
    std::size_t index = 0;
    return bonusAt(pos, index) || bonusAt({.x = pos.x + 1, .y = pos.y}, index) ||
           bonusAt({.x = pos.x, .y = pos.y + 1}, index) ||
           bonusAt({.x = pos.x + 1, .y = pos.y + 1}, index);
}

bool NibblesWorld::bonusAt(Position pos, std::size_t& index) const {
    for (std::size_t i = 0; i < bonuses_.size(); ++i) {
        const Bonus& bonus = bonuses_.at(i);
        const bool inside = pos.x >= bonus.pos.x && pos.x <= bonus.pos.x + 1 &&
                            pos.y >= bonus.pos.y && pos.y <= bonus.pos.y + 1;
        if (inside) {
            index = i;
            return true;
        }
    }
    return false;
}

bool NibblesWorld::twoByTwoEmpty(Position pos) const {
    if (pos.x < 0 || pos.y < 0 || pos.x + 1 >= level_.width || pos.y + 1 >= level_.height ||
        bonusOverlaps(pos)) {
        return false;
    }
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const Position cell{.x = pos.x + dx, .y = pos.y + dy};
            if (cellAt(cell.x, cell.y) != Cell::Empty || materializedWormAt(cell)) {
                return false;
            }
        }
    }
    return true;
}

int NibblesWorld::nearestBonusDistance(Position from) const {
    if (bonuses_.empty()) {
        return level_.width + level_.height;
    }
    int best = std::numeric_limits<int>::max();
    for (const Bonus& bonus : bonuses_) {
        best = std::min(best, manhattanWrapped(from, cellCenter(bonus)));
    }
    return best;
}

int NibblesWorld::manhattanWrapped(Position a, Position b) const {
    const int rawDx = std::abs(a.x - b.x);
    const int rawDy = std::abs(a.y - b.y);
    const int dx = level_.width > 0 ? std::min(rawDx, level_.width - rawDx) : rawDx;
    const int dy = level_.height > 0 ? std::min(rawDy, level_.height - rawDy) : rawDy;
    return dx + dy;
}

int NibblesWorld::irand(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

} // namespace og::nibbles
