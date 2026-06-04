#pragma once

#include "games/snake/SnakeConfig.hpp"
#include "games/snake/SnakeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

// The pure, SDL-free Snake simulation: a square arena of snakes (player at index
// 0, the rest bots) and food orbs. step() advances one fixed sub-step; the scene
// runs an accumulator over it. Deterministic for a given (difficulty, seed,
// per-step input) sequence — all randomness flows through one std::mt19937 — so
// it is fully unit-testable without any rendering. Mirrors the original's job
// pipeline: bot brains -> movement -> trails -> food pickup -> collisions ->
// respawn -> food upkeep.
namespace og::snake {

struct PlayerInput {
    Vec2 aimWorld; // world point the player's head should turn toward
    bool boost = false;
};

class SnakeWorld {
public:
    // difficultyIndex: 0 Easy / 1 Medium / 2 Hard (clamped). The scene maps the
    // shared Difficulty enum to this so the sim never pulls in SDL via Color.
    SnakeWorld(int difficultyIndex, std::uint32_t seed);

    void setPlayerInput(const PlayerInput& input) { input_ = input; }
    void step(); // advance one fixed kFixedDt sub-step

    [[nodiscard]] const Snake& player() const { return snakes_.front(); }
    [[nodiscard]] const std::vector<Snake>& snakes() const { return snakes_; }
    [[nodiscard]] const std::vector<FoodOrb>& food() const { return food_; }
    [[nodiscard]] bool playerAlive() const { return snakes_.front().alive; }
    [[nodiscard]] int playerScore() const { return static_cast<int>(snakes_.front().score); }
    [[nodiscard]] float worldSize() const { return worldSize_; }
    [[nodiscard]] int liveSnakeCount() const;

    // Length and radius are pure functions of score (also handy for tests/HUD).
    [[nodiscard]] static int segmentCountForScore(float score);
    [[nodiscard]] static float radiusForScore(float score);

    // ---- Test seams (harmless, also occasionally useful) --------------------
    void clearFood() { food_.clear(); }
    void addFood(const FoodOrb& orb) { food_.push_back(orb); }
    [[nodiscard]] Snake& snakeRef(std::size_t index) { return snakes_.at(index); }

private:
    void stepBotBrains();
    void steerBot(std::size_t index);
    void integrateMovement();
    void advanceTrails();
    void resolveFoodPickup();
    void resolveCollisions();
    void respawnDead();
    void maintainFood();

    static void initGeometry(Snake& snake);
    static void buildBody(Snake& snake, Vec2 at, float heading);
    void spawnBot(Snake& snake);
    void spawnFoodOrb();
    void dropDeathLoot(const Snake& dead);

    [[nodiscard]] const FoodOrb* nearestFood(Vec2 from, float radius) const;
    [[nodiscard]] const Snake* findPrey(std::size_t index, float maxScoreRatio) const;
    [[nodiscard]] static bool headHitsBody(const Snake& head, const Snake& body);
    // Which of a snake pair die from their interaction this step.
    struct PairDeaths {
        bool a = false;
        bool b = false;
    };
    [[nodiscard]] static PairDeaths resolvePair(const Snake& sa, const Snake& sb);
    [[nodiscard]] bool hitsBorder(const Snake& snake) const;
    // True if `point` lies within (snake.radius + margin) of any alive snake's
    // body/head except `excludeIndex`. Used for bot avoidance + safe spawning.
    [[nodiscard]] bool pointHitsBody(Vec2 point, std::size_t excludeIndex, float margin) const;
    // Suggested turn direction (-1/0/+1) to dodge a blocked path ahead.
    [[nodiscard]] int bodyAvoidTurn(std::size_t index) const;
    [[nodiscard]] Vec2 inwardSteer(Vec2 pos) const;
    [[nodiscard]] Vec2 randomSafePosition(float clearance);
    [[nodiscard]] std::string_view pickName();
    FoodOrb makeWeightedFood(Vec2 pos);

    [[nodiscard]] float frand(float lo, float hi);
    [[nodiscard]] int irand(int lo, int hi);

    int difficultyIndex_;
    std::mt19937 rng_;
    std::vector<Snake> snakes_;
    std::vector<FoodOrb> food_;
    PlayerInput input_{};
    float worldSize_ = config::kWorldSize;
};

} // namespace og::snake
