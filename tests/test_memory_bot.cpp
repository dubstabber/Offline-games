#include "games/Difficulty.hpp"
#include "games/memory/MemoryBoard.hpp"
#include "games/memory/MemoryBot.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <random>
#include <vector>

namespace {

using og::Difficulty;
using og::MemoryBoard;
using og::MemoryBot;
using og::MemoryBotProfile;
using og::memoryBotProfile;
using Outcome = MemoryBoard::Outcome;
using Player = MemoryBoard::Player;

constexpr MemoryBotProfile kPerfect{.recallLow = 1.0F, .recallHigh = 1.0F};
constexpr MemoryBotProfile kGoldfish{.recallLow = 0.0F, .recallHigh = 0.0F};

bool near(float a, float b) {
    return std::abs(a - b) < 1e-4F;
}

int partnerOf(const MemoryBoard& board, int index) {
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != index && board.card(i).picture == board.card(index).picture) {
            return i;
        }
    }
    assert(false && "every picture is dealt twice");
    return -1;
}

int mismatchFor(const MemoryBoard& board, int index) {
    for (int i = 0; i < board.cardCount(); ++i) {
        if (!board.card(i).matched && !board.card(i).faceUp &&
            board.card(i).picture != board.card(index).picture) {
            return i;
        }
    }
    assert(false && "expected a second picture on the table");
    return -1;
}

int firstOnTable(const MemoryBoard& board) {
    for (int i = 0; i < board.cardCount(); ++i) {
        if (!board.card(i).matched) {
            return i;
        }
    }
    return -1;
}

// Flip two cards with the bot watching, then settle them.
Outcome playTurn(MemoryBoard& board, MemoryBot& bot, int a, int b) {
    assert(board.flip(a));
    bot.observe(board, a);
    assert(board.flip(b));
    bot.observe(board, b);
    return board.resolve();
}

// Harder tiers remember more, and VeryHard folds into Hard.
void testProfiles() {
    const MemoryBotProfile easy = memoryBotProfile(Difficulty::Easy);
    const MemoryBotProfile medium = memoryBotProfile(Difficulty::Medium);
    const MemoryBotProfile hard = memoryBotProfile(Difficulty::Hard);
    for (const MemoryBotProfile& p : {easy, medium, hard}) {
        assert(p.recallLow >= 0.0F && p.recallLow <= p.recallHigh && p.recallHigh <= 1.0F);
    }
    assert(easy.recallHigh < medium.recallHigh && medium.recallHigh < hard.recallHigh);
    assert(easy.recallLow < medium.recallLow && medium.recallLow < hard.recallLow);
    assert(near(memoryBotProfile(Difficulty::VeryHard).recallHigh, hard.recallHigh));
}

// Recall sits mid-range at level scores and slides to the ends of the range as
// one side pulls kExtremeLead pairs ahead (the player leading sharpens the bot).
void testRubberBand() {
    const MemoryBotProfile profile{.recallLow = 0.2F, .recallHigh = 0.8F};
    MemoryBot bot(profile, 1);
    MemoryBoard board(15, 3);
    assert(near(bot.recallProbability(board), 0.5F));

    // The player runs off kExtremeLead pairs: the bot is at its sharpest.
    for (int i = 0; i < MemoryBot::kExtremeLead; ++i) {
        const int a = firstOnTable(board);
        assert(playTurn(board, bot, a, partnerOf(board, a)) == Outcome::Match);
    }
    assert(near(bot.recallProbability(board), 0.8F));
    {
        const int a = firstOnTable(board);
        assert(playTurn(board, bot, a, mismatchFor(board, a)) == Outcome::Mismatch);
    }
    // Now the bot (to move) claws back twice the lead: it relaxes to the floor.
    assert(board.turn() == Player::Bot);
    for (int i = 0; i < 2 * MemoryBot::kExtremeLead; ++i) {
        const int a = firstOnTable(board);
        assert(playTurn(board, bot, a, partnerOf(board, a)) == Outcome::Match);
    }
    assert(board.score(Player::Bot) - board.score(Player::You) == MemoryBot::kExtremeLead);
    assert(near(bot.recallProbability(board), 0.2F));
}

// With perfect recall every observed card is remembered, once.
void testObserve() {
    MemoryBot bot(kPerfect, 5);
    MemoryBoard board(7, 5);
    assert(bot.knownCount() == 0);
    assert(!bot.knows(0));
    assert(board.flip(0));
    bot.observe(board, 0);
    assert(bot.knows(0));
    assert(bot.knownCount() == 1);
    bot.observe(board, 0);
    assert(bot.knownCount() == 1);
    bot.observe(board, -1);
    bot.observe(board, board.cardCount());
    assert(bot.knownCount() == 1);
    bot.reset();
    assert(bot.knownCount() == 0);
    assert(!bot.knows(0));
}

// A bot that saw both halves of a pair in earlier turns flips exactly that pair.
void testTakesRememberedPair() {
    MemoryBot bot(kPerfect, 9);
    MemoryBoard board(11, 9);
    const int a = 0;
    const int a2 = partnerOf(board, a);
    // Player: a and a mismatch. Bot (played by hand): a's partner and a mismatch.
    assert(playTurn(board, bot, a, mismatchFor(board, a)) == Outcome::Mismatch);
    assert(board.turn() == Player::Bot);
    assert(playTurn(board, bot, a2, mismatchFor(board, a2)) == Outcome::Mismatch);
    assert(board.turn() == Player::You);
    // Player misses again with two other cards; now the bot chooses for itself.
    int c = -1;
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != a && i != a2 && board.card(i).picture != board.card(a).picture) {
            c = i;
            break;
        }
    }
    assert(c >= 0);
    int d = -1;
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != a && i != a2 && i != c && board.card(i).picture != board.card(a).picture &&
            board.card(i).picture != board.card(c).picture) {
            d = i;
            break;
        }
    }
    assert(d >= 0);
    assert(playTurn(board, bot, c, d) == Outcome::Mismatch);
    assert(board.turn() == Player::Bot);

    const int first = bot.chooseFirst(board);
    assert(first == a || first == a2);
    assert(board.flip(first));
    bot.observe(board, first);
    const int second = bot.chooseSecond(board, first);
    assert(second == (first == a ? a2 : a));
    assert(board.flip(second));
    bot.observe(board, second);
    assert(board.resolve() == Outcome::Match);
    assert(board.score(Player::Bot) == 1);
}

// The second flip goes to the remembered partner of whatever the first showed.
void testSecondFlipUsesMemory() {
    MemoryBot bot(kPerfect, 2);
    MemoryBoard board(11, 2);
    const int a = 0;
    assert(playTurn(board, bot, a, mismatchFor(board, a)) == Outcome::Mismatch);
    assert(board.turn() == Player::Bot);
    const int a2 = partnerOf(board, a);
    assert(board.flip(a2)); // suppose the bot's exploration lands on a's partner
    bot.observe(board, a2);
    assert(bot.chooseSecond(board, a2) == a);
}

// A bot that remembers nothing still always flips legal, distinct cards.
void testGoldfishIsLegal() {
    MemoryBot bot(kGoldfish, 4);
    MemoryBoard board(7, 4);
    for (int round = 0; round < 40 && !board.isOver(); ++round) {
        const int first = bot.chooseFirst(board);
        assert(board.canFlip(first));
        assert(board.flip(first));
        bot.observe(board, first);
        const int second = bot.chooseSecond(board, first);
        assert(second != first);
        assert(board.canFlip(second));
        assert(board.flip(second));
        bot.observe(board, second);
        board.resolve();
        assert(bot.knownCount() == 0);
    }
}

// A whole game per difficulty: a random player against a perfect bot. Every bot
// choice is legal, the bot never misses a partner it has seen, and the game
// ends with every pair collected.
void testFullGames() {
    for (const int pairs : {7, 11, 15}) {
        std::mt19937 playerRng(static_cast<std::uint32_t>(pairs));
        MemoryBot bot(kPerfect, 77);
        MemoryBoard board(pairs, 77);
        std::vector<bool> seen(static_cast<std::size_t>(board.cardCount()), false);
        const auto randomTableCard = [&](int exclude) {
            std::vector<int> options;
            for (int i = 0; i < board.cardCount(); ++i) {
                if (i != exclude && board.canFlip(i)) {
                    options.push_back(i);
                }
            }
            assert(!options.empty());
            std::uniform_int_distribution<std::size_t> dist(0, options.size() - 1);
            return options[dist(playerRng)];
        };
        const auto reveal = [&](int index) {
            assert(board.flip(index));
            bot.observe(board, index);
            seen[static_cast<std::size_t>(index)] = true;
        };
        int turns = 0;
        while (!board.isOver()) {
            assert(++turns < 1000);
            if (board.turn() == Player::You) {
                const int a = randomTableCard(-1);
                reveal(a);
                reveal(randomTableCard(a));
            } else {
                const int first = bot.chooseFirst(board);
                assert(board.canFlip(first));
                reveal(first);
                const int second = bot.chooseSecond(board, first);
                assert(second != first && board.canFlip(second));
                const int partner = partnerOf(board, first);
                if (seen[static_cast<std::size_t>(partner)]) {
                    assert(second == partner); // a seen partner is never passed over
                }
                reveal(second);
            }
            board.resolve();
        }
        assert(board.score(Player::You) + board.score(Player::Bot) == pairs);
        for (int i = 0; i < board.cardCount(); ++i) {
            assert(board.card(i).matched);
        }
    }
}

} // namespace

int main() {
    testProfiles();
    testRubberBand();
    testObserve();
    testTakesRememberedPair();
    testSecondFlipUsesMemory();
    testGoldfishIsLegal();
    testFullGames();
    std::puts("memory_bot: all tests passed");
    return 0;
}
