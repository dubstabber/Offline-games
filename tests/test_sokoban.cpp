#include "games/sokoban/SokobanBoard.hpp"
#include "games/sokoban/SokobanLevels.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <span>
#include <string_view>
#include <vector>

namespace {

using og::kSokobanTierCount;
using og::parseSokobanLevels;
using og::SokobanBoard;
using og::SokobanLevel;

[[nodiscard]] std::vector<char> flatten(std::initializer_list<std::string_view> rows) {
    std::vector<char> cells;
    for (std::string_view row : rows) {
        cells.insert(cells.end(), row.begin(), row.end());
    }
    return cells;
}

[[nodiscard]] SokobanBoard makeBoard(std::initializer_list<std::string_view> rows) {
    const int height = static_cast<int>(rows.size());
    const int width = rows.size() == 0 ? 0 : static_cast<int>(rows.begin()->size());
    const std::vector<char> cells = flatten(rows);
    return {width, height, std::span<const char>(cells.data(), cells.size())};
}

void testMoveWithoutPush() {
    SokobanBoard board = makeBoard({"#####", "#@  #", "# $.#", "#####"});
    assert(board.tryMove(SokobanBoard::Direction::Right));
    assert((board.player() == SokobanBoard::Cell{2, 1}));
    assert(board.moves() == 1);
    assert(board.pushes() == 0);
    assert(board.hasBox(2, 2));
    assert(!board.isSolved());
}

void testPushOntoEmptyFloor() {
    SokobanBoard board = makeBoard({"######", "#@$ .#", "######"});
    assert(board.tryMove(SokobanBoard::Direction::Right));
    assert((board.player() == SokobanBoard::Cell{2, 1}));
    assert(board.hasBox(3, 1));
    assert(board.moves() == 1);
    assert(board.pushes() == 1);
    assert(!board.isSolved());
}

void testBlockedByWall() {
    SokobanBoard board = makeBoard({"#####", "#@#.#", "#####"});
    assert(!board.tryMove(SokobanBoard::Direction::Right));
    assert((board.player() == SokobanBoard::Cell{1, 1}));
    assert(board.moves() == 0);
}

void testBlockedPushIntoWall() {
    SokobanBoard board = makeBoard({"#####", "#@$##", "#####"});
    assert(!board.tryMove(SokobanBoard::Direction::Right));
    assert(board.hasBox(2, 1));
    assert(board.moves() == 0);
    assert(board.pushes() == 0);
}

void testBlockedPushIntoBox() {
    SokobanBoard board = makeBoard({"######", "#@$$.#", "######"});
    assert(!board.tryMove(SokobanBoard::Direction::Right));
    assert(board.hasBox(2, 1));
    assert(board.hasBox(3, 1));
    assert(board.moves() == 0);
}

void testSolvedAndUndo() {
    SokobanBoard board = makeBoard({"#####", "#@$.#", "#####"});
    assert(board.boxCount() == 1);
    assert(board.goalCount() == 1);
    assert(!board.isSolved());
    assert(board.tryMove(SokobanBoard::Direction::Right));
    assert(board.isSolved());
    assert(board.hasBox(3, 1));
    assert(board.moves() == 1);
    assert(board.pushes() == 1);
    assert(board.canUndo());

    assert(board.undo());
    assert(!board.isSolved());
    assert((board.player() == SokobanBoard::Cell{1, 1}));
    assert(board.hasBox(2, 1));
    assert(board.moves() == 0);
    assert(board.pushes() == 0);
    assert(!board.canUndo());
    assert(!board.undo());
}

void testReset() {
    SokobanBoard board = makeBoard({"######", "#@$ .#", "######"});
    assert(board.tryMove(SokobanBoard::Direction::Right));
    assert(board.tryMove(SokobanBoard::Direction::Right));
    assert(board.moves() == 2);
    board.reset();
    assert((board.player() == SokobanBoard::Cell{1, 1}));
    assert(board.hasBox(2, 1));
    assert(board.moves() == 0);
    assert(board.pushes() == 0);
    assert(!board.canUndo());
}

void testEncodedGoalCells() {
    SokobanBoard solved = makeBoard({"#####", "#@* #", "#####"});
    assert(solved.isGoal(2, 1));
    assert(solved.hasBox(2, 1));
    assert(solved.isSolved());

    SokobanBoard playerOnGoal = makeBoard({"#####", "#+$ #", "#####"});
    assert(playerOnGoal.isGoal(1, 1));
    assert((playerOnGoal.player() == SokobanBoard::Cell{1, 1}));
    assert(!playerOnGoal.isSolved());
}

void testParserSynthetic() {
    const std::vector<std::uint8_t> blob = {
        0x53, 0x4F, 0x4B, 0x4F, // magic "SOKO"
        0x01, 0x00,             // version 1
        0x03, 0x00,             // tierCount 3
        0x01, 0x00,             // count[0] = 1
        0x00, 0x00,             // count[1] = 0
        0x00, 0x00,             // count[2] = 0
        0x03, 0x01, 0x07,       // w=3 h=1 sourceSet=7
        0x02, 0x00,             // sourceLevel = 2
        '@',  '$',  '.',        // cells
    };
    const std::array<std::vector<SokobanLevel>, kSokobanTierCount> tiers = parseSokobanLevels(blob);
    assert(tiers.at(0).size() == 1);
    assert(tiers.at(1).empty() && tiers.at(2).empty());
    const SokobanLevel& level = tiers.at(0).at(0);
    assert(level.width == 3);
    assert(level.height == 1);
    assert(level.sourceSet == 7);
    assert(level.sourceLevel == 2);
    assert(level.cells.size() == 3);
    assert(level.boxCount() == 1);
    assert(level.goalCount() == 1);
}

void testParserRobustness() {
    auto allEmpty = [](const std::array<std::vector<SokobanLevel>, kSokobanTierCount>& tiers) {
        return std::ranges::all_of(tiers, [](const auto& tier) { return tier.empty(); });
    };
    assert(allEmpty(parseSokobanLevels(std::vector<std::uint8_t>{})));
    assert(allEmpty(parseSokobanLevels(std::vector<std::uint8_t>{0, 0, 0, 0, 1, 0, 3, 0})));

    const std::vector<std::uint8_t> truncated = {
        0x53, 0x4F, 0x4B, 0x4F, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    assert(allEmpty(parseSokobanLevels(truncated)));
}

[[nodiscard]] std::vector<std::uint8_t> readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    f.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<char> raw(size);
    f.read(raw.data(), static_cast<std::streamsize>(size));
    return {raw.begin(), raw.end()};
}

[[nodiscard]] bool validCell(char cell) {
    return cell == ' ' || cell == '#' || cell == '.' || cell == '$' || cell == '*' || cell == '@' ||
           cell == '+';
}

void assertTierCounts(const std::array<std::vector<SokobanLevel>, kSokobanTierCount>& tiers) {
    assert(tiers.at(0).size() == 314);
    assert(tiers.at(1).size() == 150);
    assert(tiers.at(2).size() == 200);
}

[[nodiscard]] int countPlayers(const SokobanLevel& level) {
    return static_cast<int>(
        std::ranges::count_if(level.cells, [](char cell) { return cell == '@' || cell == '+'; }));
}

void assertLevelWellFormed(const SokobanLevel& level) {
    assert(level.width >= 3);
    assert(level.width <= 64);
    assert(level.height >= 1);
    assert(level.height <= 64);
    assert(level.sourceSet >= 0);
    assert(level.sourceSet < 10);
    assert(level.sourceLevel >= 1);
    assert(level.cells.size() == static_cast<std::size_t>(level.width * level.height));
    assert(std::ranges::all_of(level.cells, validCell));
    assert(countPlayers(level) == 1);
    assert(level.boxCount() >= 1);
    assert(level.boxCount() == level.goalCount());
}

[[nodiscard]] std::size_t
assertAllLevelsWellFormed(const std::array<std::vector<SokobanLevel>, kSokobanTierCount>& tiers) {
    std::size_t total = 0;
    for (const std::vector<SokobanLevel>& tier : tiers) {
        total += tier.size();
        for (const SokobanLevel& level : tier) {
            assertLevelWellFormed(level);
        }
    }
    return total;
}

void testRealAsset() {
#ifdef SOKOBAN_LEVELS_PATH
    const std::vector<std::uint8_t> bytes = readFile(SOKOBAN_LEVELS_PATH);
    if (bytes.empty()) {
        std::printf("WARNING: could not read %s; skipping real-asset gate\n", SOKOBAN_LEVELS_PATH);
        return;
    }
    const std::array<std::vector<SokobanLevel>, kSokobanTierCount> tiers =
        parseSokobanLevels(bytes);
    assertTierCounts(tiers);
    const std::size_t total = assertAllLevelsWellFormed(tiers);
    assert(total == 664);
    std::printf("Decoded %zu Sokoban levels (%zu/%zu/%zu); all well-formed.\n", total,
                tiers.at(0).size(), tiers.at(1).size(), tiers.at(2).size());
#else
    std::printf("SOKOBAN_LEVELS_PATH not defined; skipping real-asset gate.\n");
#endif
}

} // namespace

int main() {
    testMoveWithoutPush();
    testPushOntoEmptyFloor();
    testBlockedByWall();
    testBlockedPushIntoWall();
    testBlockedPushIntoBox();
    testSolvedAndUndo();
    testReset();
    testEncodedGoalCells();
    testParserSynthetic();
    testParserRobustness();
    testRealAsset();
    std::puts("All Sokoban tests passed.");
    return 0;
}
