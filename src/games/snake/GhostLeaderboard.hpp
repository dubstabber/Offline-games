#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

// A synthetic global leaderboard. The world only simulates ~20 real snakes, but
// the original's HUD shows a populous board (top scorers in the tens of thousands,
// the player ranked ~181 of ~240). This holds a few hundred "ghost" competitors
// whose scores drift slowly over time (never simulated as real snakes) and merges
// the real snakes + player into them by score to produce the ranked view. Pure /
// SDL-free; cosmetic only (it never affects gameplay).
namespace og::snake {

// A live snake to merge into the board (name + current score).
struct RealEntry {
    std::string_view name;
    int score = 0;
};

struct LeaderRow {
    std::string name;
    int score = 0;
    bool isPlayer = false;
};

struct LeaderboardView {
    std::vector<LeaderRow> top; // highest `topN` rows, descending
    int playerRank = 0;         // player's 1-based global rank
    int total = 0;              // total competitors (ghosts + reals + player)
    int playerScore = 0;
};

class GhostLeaderboard {
public:
    GhostLeaderboard(std::uint32_t seed, int ghostCount);

    // Drift the ghost scores (wall-clock dt is fine — purely cosmetic).
    void advance(float dtSeconds);

    [[nodiscard]] LeaderboardView build(int topN, std::string_view playerName, int playerScore,
                                        const std::vector<RealEntry>& reals) const;

private:
    struct Ghost {
        std::string name;
        float score = 0.0F;
        float drift = 0.0F;
        float timer = 0.0F;
    };

    [[nodiscard]] float frand(float lo, float hi);

    std::mt19937 rng_;
    std::vector<Ghost> ghosts_;
};

} // namespace og::snake
