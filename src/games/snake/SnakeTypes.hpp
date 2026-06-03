#pragma once

#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>

// Pure, SDL-free value types for the Snake game. A small local Vec2 (the project
// has no shared 2D vector) plus the food/snake/bot data the simulation owns. No
// rendering, no colors here — snakes carry only palette *indices* so the logic
// stays SDL-free and unit-testable (see SnakePalette.hpp for the actual colors).
namespace og::snake {

// A 2D vector. The explicit constructor (rather than a bare aggregate) keeps the
// many positional `Vec2{x, y}` call sites concise without tripping clang-tidy's
// designated-initializer check.
struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;

    constexpr Vec2() = default;
    constexpr Vec2(float xv, float yv) : x(xv), y(yv) {}
};

[[nodiscard]] constexpr Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
[[nodiscard]] constexpr Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
[[nodiscard]] constexpr Vec2 operator*(Vec2 v, float s) {
    return {v.x * s, v.y * s};
}

[[nodiscard]] constexpr float dot(Vec2 a, Vec2 b) {
    return (a.x * b.x) + (a.y * b.y);
}
[[nodiscard]] constexpr float lengthSq(Vec2 v) {
    return dot(v, v);
}
[[nodiscard]] inline float length(Vec2 v) {
    return std::sqrt(lengthSq(v));
}
[[nodiscard]] constexpr float distSq(Vec2 a, Vec2 b) {
    return lengthSq(b - a);
}

[[nodiscard]] inline Vec2 normalize(Vec2 v) {
    const float len = length(v);
    return len > 1e-6F ? Vec2{v.x / len, v.y / len} : Vec2{0.0F, 0.0F};
}

[[nodiscard]] inline float angleOf(Vec2 v) {
    return std::atan2(v.y, v.x);
}
[[nodiscard]] inline Vec2 fromAngle(float radians) {
    return {std::cos(radians), std::sin(radians)};
}

// Wrap an angle into (-pi, pi].
[[nodiscard]] inline float wrapAngle(float radians) {
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr float kTwoPi = 2.0F * kPi;
    float a = std::fmod(radians + kPi, kTwoPi);
    if (a < 0.0F) {
        a += kTwoPi;
    }
    return a - kPi;
}

// Turn `current` toward `target` by at most `maxStep` radians, the short way.
[[nodiscard]] inline float rotateToward(float current, float target, float maxStep) {
    const float delta = wrapAngle(target - current);
    if (std::fabs(delta) <= maxStep) {
        return wrapAngle(target);
    }
    return wrapAngle(current + (delta > 0.0F ? maxStep : -maxStep));
}

// A single food orb. `mass` adds to a snake's score when eaten; `radius` and
// `colorIndex` (into SnakePalette::kFoodColors) are cosmetic, cached at spawn.
struct FoodOrb {
    Vec2 pos;
    float mass = 1.0F;
    float radius = 0.0F;
    std::uint8_t colorIndex = 0;
};

// A bot's "personality": how hard it avoids crashing, how readily it hunts, how
// fast it can turn, how often it boosts, and how far it looks for food. Derived
// from a per-difficulty preset (see SnakeConfig::botPresetFor) with jitter.
struct BotConfig {
    float crashAvoidance = 1.0F;
    float aggressiveness = 0.0F;
    float maxTurn = 5.0F;   // rad/s
    float boostBias = 0.0F; // 0..1 tendency to boost while chasing
    float foodSeekRadius = 1000.0F;
};

// One snake (player at index 0, the rest bots). The body is a path-history of
// head samples spaced `spacing` apart (newest at front); discs drawn/collided at
// those samples form the visible tube. `score` is the single source of truth for
// length/radius (pure functions of it). `gradA/gradB` index SnakePalette tables.
struct Snake {
    Vec2 head;
    float heading = 0.0F;       // current facing (rad)
    float targetHeading = 0.0F; // desired facing (rad)
    float speed = 0.0F;         // units/s this step
    bool boosting = false;
    float score = 0.0F;
    float radius = 0.0F;
    float spacing = 0.0F;
    std::deque<Vec2> path;  // head samples, index 0 = newest (just behind head)
    float pathAccum = 0.0F; // distance travelled since the last path sample
    float boostDropAccum = 0.0F;
    float invulnTimer = 0.0F; // seconds of spawn invulnerability left
    bool alive = true;
    bool isBot = true;
    BotConfig bot;
    std::uint8_t gradIndex = 0; // into SnakePalette::kSnakeGradients
    std::string name;
};

} // namespace og::snake
