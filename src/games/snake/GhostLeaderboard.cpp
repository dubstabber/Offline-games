#include "games/snake/GhostLeaderboard.hpp"

#include "games/snake/SnakeConfig.hpp"

#include <algorithm>
#include <cmath>

namespace og::snake {
namespace {

constexpr float kMaxGhostScore = 28000.0F; // top of the board, like the original
constexpr float kMinGhostScore = 12.0F;

} // namespace

GhostLeaderboard::GhostLeaderboard(std::uint32_t seed, int ghostCount) : rng_(seed) {
    ghosts_.reserve(static_cast<std::size_t>(std::max(0, ghostCount)));
    for (int i = 0; i < ghostCount; ++i) {
        const float u = frand(0.0F, 1.0F);
        // Cubic skew: most ghosts are low, a few reach the top scores.
        const float score = std::max(frand(20.0F, 240.0F), kMaxGhostScore * u * u * u);
        Ghost ghost;
        ghost.name = std::string(
            config::kNamePool.at(static_cast<std::size_t>(i) % config::kNamePool.size()));
        ghost.score = score;
        ghost.drift = frand(-6.0F, 8.0F);
        ghost.timer = frand(1.0F, 6.0F);
        ghosts_.push_back(std::move(ghost));
    }
}

void GhostLeaderboard::advance(float dtSeconds) {
    for (Ghost& ghost : ghosts_) {
        ghost.timer -= dtSeconds;
        if (ghost.timer <= 0.0F) {
            const float swing = (ghost.score * 0.02F) + 6.0F;
            ghost.drift = frand(-swing, swing + 4.0F); // gentle upward bias
            ghost.timer = frand(2.0F, 6.0F);
        }
        ghost.score =
            std::clamp(ghost.score + (ghost.drift * dtSeconds), kMinGhostScore, kMaxGhostScore);
    }
}

LeaderboardView GhostLeaderboard::build(int topN, std::string_view playerName, int playerScore,
                                        const std::vector<RealEntry>& reals) const {
    struct Item {
        int score;
        std::string_view name;
        bool isPlayer;
    };
    std::vector<Item> items;
    items.reserve(ghosts_.size() + reals.size() + 1);
    for (const Ghost& ghost : ghosts_) {
        items.push_back(
            {.score = static_cast<int>(ghost.score), .name = ghost.name, .isPlayer = false});
    }
    for (const RealEntry& real : reals) {
        items.push_back({.score = real.score, .name = real.name, .isPlayer = false});
    }
    items.push_back({.score = playerScore, .name = playerName, .isPlayer = true});

    std::ranges::stable_sort(items, [](const Item& a, const Item& b) { return a.score > b.score; });

    LeaderboardView view;
    view.total = static_cast<int>(items.size());
    view.playerScore = playerScore;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items.at(i).isPlayer) {
            view.playerRank = static_cast<int>(i) + 1;
            break;
        }
    }
    const std::size_t rows = std::min(static_cast<std::size_t>(std::max(0, topN)), items.size());
    view.top.reserve(rows);
    for (std::size_t i = 0; i < rows; ++i) {
        const Item& item = items.at(i);
        view.top.push_back(
            {.name = std::string(item.name), .score = item.score, .isPlayer = item.isPlayer});
    }
    return view;
}

float GhostLeaderboard::frand(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

} // namespace og::snake
