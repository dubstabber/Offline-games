#include "games/nibbles/NibblesLevels.hpp"
#include "games/nibbles/NibblesWorld.hpp"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <span>
#include <vector>

namespace {

using og::nibbles::Bonus;
using og::nibbles::BonusType;
using og::nibbles::Cell;
using og::nibbles::Direction;
using og::nibbles::NibblesConfig;
using og::nibbles::NibblesLevel;
using og::nibbles::NibblesStatus;
using og::nibbles::NibblesWorld;
using og::nibbles::Position;
using og::nibbles::Spawn;
using og::nibbles::Warp;

NibblesLevel makeLevel() {
    NibblesLevel level;
    level.width = 12;
    level.height = 10;
    level.sourceLevel = 1;
    level.cells.assign(static_cast<std::size_t>(level.width) *
                           static_cast<std::size_t>(level.height),
                       Cell::Empty);
    level.spawns = {
        Spawn{.pos = {.x = 3, .y = 5}, .direction = Direction::Right},
        Spawn{.pos = {.x = 8, .y = 2}, .direction = Direction::Left},
        Spawn{.pos = {.x = 8, .y = 7}, .direction = Direction::Left},
        Spawn{.pos = {.x = 3, .y = 2}, .direction = Direction::Right},
        Spawn{.pos = {.x = 3, .y = 7}, .direction = Direction::Right},
        Spawn{.pos = {.x = 8, .y = 5}, .direction = Direction::Left},
    };
    return level;
}

NibblesConfig oneWormConfig() {
    return {.wormCount = 1, .tickMs = 100, .fakes = true, .regularBonusCount = 1};
}

NibblesWorld makeWorld() {
    NibblesWorld world(makeLevel(), oneWormConfig(), 123U);
    world.clearBonuses();
    world.setRegularLeft(99);
    return world;
}

void testBundledLevelsParse() {
    std::ifstream file(NIBBLES_LEVELS_PATH, std::ios::binary);
    assert(file.good());
    std::vector<std::uint8_t> raw;
    char byte = 0;
    while (file.get(byte)) {
        raw.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    const std::vector<NibblesLevel> levels =
        og::nibbles::parseNibblesLevels(std::span<const std::uint8_t>(raw.data(), raw.size()));
    assert(levels.size() == 26);
    for (int i = 0; i < 26; ++i) {
        const NibblesLevel& level = levels.at(static_cast<std::size_t>(i));
        assert(level.sourceLevel == i + 1);
        assert(level.width == 92);
        assert(level.height == 66);
        assert(level.cells.size() == static_cast<std::size_t>(92 * 66));
        assert(!level.spawns.empty());
    }
}

void testMovesAndTurns() {
    NibblesWorld world = makeWorld();
    const Position start{.x = 3, .y = 5};
    const Position oneStep{.x = 4, .y = 5};
    const Position turned{.x = 4, .y = 6};
    assert(world.worms().front().head() == start);
    world.step();
    assert(world.worms().front().head() == oneStep);

    world.queueTurn(Direction::Down);
    world.step();
    assert(world.worms().front().head() == turned);
}

void testWallCollisionCostsLife() {
    NibblesWorld world = makeWorld();
    world.setCell(4, 5, Cell::Wall);
    world.step();
    assert(world.lives() == 5);
    assert(world.status() == NibblesStatus::Playing);
    assert(world.worms().front().alive());
}

void testRegularBonusCompletesLevel() {
    NibblesWorld world = makeWorld();
    world.setRegularLeft(1);
    world.addBonus(Bonus{
        .type = BonusType::Regular, .pos = {.x = 4, .y = 5}, .fake = false, .countdown = 100});
    world.step();
    assert(world.status() == NibblesStatus::LevelComplete);
    assert(world.regularLeft() == 0);
    assert(world.score() > 0);
}

void testFakeBonusReversesWithoutScore() {
    NibblesWorld world = makeWorld();
    world.addBonus(
        Bonus{.type = BonusType::Regular, .pos = {.x = 4, .y = 5}, .fake = true, .countdown = 100});
    world.step();
    const Position reversedHead{.x = 0, .y = 5};
    assert(world.score() == 0);
    assert(world.worms().front().direction == Direction::Left);
    assert(world.worms().front().head() == reversedHead);
}

void testWarpMovesToTarget() {
    NibblesLevel level = makeLevel();
    for (int y = 5; y <= 6; ++y) {
        for (int x = 4; x <= 5; ++x) {
            const auto index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(level.width)) +
                static_cast<std::size_t>(x);
            level.cells.at(index) = Cell::Warp;
        }
    }
    level.warps.push_back(Warp{.id = 1,
                               .source = {.x = 4, .y = 5},
                               .target = {.x = 8, .y = 5},
                               .random = false,
                               .bidirectional = false});
    NibblesWorld world(level, oneWormConfig(), 77U);
    world.clearBonuses();
    world.setRegularLeft(99);
    world.step();
    const Position target{.x = 8, .y = 5};
    assert(world.worms().front().head() == target);
}

void testDeterminism() {
    NibblesWorld a = makeWorld();
    NibblesWorld b = makeWorld();
    a.addBonus(Bonus{
        .type = BonusType::Regular, .pos = {.x = 6, .y = 5}, .fake = false, .countdown = 100});
    b.addBonus(Bonus{
        .type = BonusType::Regular, .pos = {.x = 6, .y = 5}, .fake = false, .countdown = 100});
    for (int i = 0; i < 20; ++i) {
        if (i == 3) {
            a.queueTurn(Direction::Down);
            b.queueTurn(Direction::Down);
        }
        a.step();
        b.step();
        assert(a.worms().front().head() == b.worms().front().head());
        assert(a.score() == b.score());
        assert(a.lives() == b.lives());
    }
}

} // namespace

int main() {
    testBundledLevelsParse();
    testMovesAndTurns();
    testWallCollisionCostsLife();
    testRegularBonusCompletesLevel();
    testFakeBonusReversesWithoutScore();
    testWarpMovesToTarget();
    testDeterminism();
    return 0;
}
