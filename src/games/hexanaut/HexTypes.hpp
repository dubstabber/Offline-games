#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

// Pure, SDL-free value types and math for the Hexanaut game: a small local Vec2
// (the project keeps per-game vectors local — see SnakeTypes.hpp), flat-top hex
// coordinate math, and the player-id sentinels the grid uses. No rendering or
// colors here, so the simulation stays unit-testable.
namespace og::hexanaut {

// Players are 0..N-1 (id 0 is the human); 0xFF is the "nobody" sentinel reused
// for both an unclaimed cell and a cell with no active trail.
using PlayerId = std::uint8_t;
inline constexpr PlayerId kNeutral = 0xFF;
inline constexpr PlayerId kNoTrail = 0xFF;

// Map items. Stored in Cell::powerup as the underlying byte (0 = none). Speed and
// Vision are collectible buffs (picked up by stepping on them). Shooter and
// SlowTotem are persistent, territory-bound items: never picked up — they act for
// whoever currently owns the cell they sit on, and can be stolen by capturing that
// cell. Shooter captures nearby land with lasers (HexWorld::updateShooters);
// SlowTotem slows every other avatar standing in its field (see inEnemySlowField).
enum class PowerUp : std::uint8_t { None, Speed, Vision, Shooter, SlowTotem };

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
[[nodiscard]] constexpr Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return {a.x + ((b.x - a.x) * t), a.y + ((b.y - a.y) * t)};
}
[[nodiscard]] inline float length(Vec2 v) {
    return std::sqrt((v.x * v.x) + (v.y * v.y));
}
[[nodiscard]] inline float angleOf(Vec2 v) {
    return std::atan2(v.y, v.x);
}

// Axial hex coordinate (flat-top). Stored densely in HexGrid with q in [0,width)
// and r in [0,height).
struct HexCoord {
    int q = 0;
    int r = 0;
};

[[nodiscard]] constexpr bool operator==(HexCoord a, HexCoord b) {
    return a.q == b.q && a.r == b.r;
}
[[nodiscard]] constexpr bool operator!=(HexCoord a, HexCoord b) {
    return !(a == b);
}

// Flat-top hexes have vertical (N/S) neighbors plus four diagonals; there is no
// pure E/W neighbor (those are the pointy corners). This ENUM ORDER fixes the
// neighbor table and is relied on for determinism everywhere — do not reorder.
enum class HexDir : std::uint8_t { N, NE, SE, S, SW, NW, None };

inline constexpr std::array<std::pair<int, int>, 6> kHexDirs{{
    {0, -1},  // N
    {+1, -1}, // NE
    {+1, 0},  // SE
    {0, +1},  // S
    {-1, +1}, // SW
    {-1, 0},  // NW
}};

[[nodiscard]] constexpr HexCoord neighbor(HexCoord c, HexDir d) {
    const auto [dq, dr] = kHexDirs.at(static_cast<std::size_t>(d));
    return {c.q + dq, c.r + dr};
}

// Opposite direction: N<->S, NE<->SW, SE<->NW. Used to forbid 180° reversal.
[[nodiscard]] constexpr HexDir opposite(HexDir d) {
    return static_cast<HexDir>((static_cast<std::uint8_t>(d) + 3U) % 6U);
}

[[nodiscard]] constexpr int iabs(int v) {
    return v < 0 ? -v : v;
}

// Hex (cube) distance between two axial coords.
[[nodiscard]] constexpr int hexDistance(HexCoord a, HexCoord b) {
    const int ax = a.q;
    const int az = a.r;
    const int ay = -ax - az;
    const int bx = b.q;
    const int bz = b.r;
    const int by = -bx - bz;
    return (iabs(ax - bx) + iabs(ay - by) + iabs(az - bz)) / 2;
}

// Flat-top axial -> world-pixel center (the logic plane, pre-projection). `size`
// is the hex's center-to-corner radius.
[[nodiscard]] inline Vec2 axialToWorld(HexCoord c, float size) {
    const auto fq = static_cast<float>(c.q);
    const auto fr = static_cast<float>(c.r);
    return {1.5F * size * fq, std::numbers::sqrt3_v<float> * size * (fr + (fq * 0.5F))};
}

// Snap a world-space heading angle (radians) to the nearest of the 6 flat-top
// directions. The directions sit at -90,-30,30,90,150,210 degrees (= N,NE,SE,S,
// SW,NW), i.e. index k at angle (-90 + 60*k).
[[nodiscard]] inline HexDir quantizeToHexDir(float angleRad) {
    constexpr float kRadToDeg = 180.0F / std::numbers::pi_v<float>;
    const float deg = angleRad * kRadToDeg;
    int k = static_cast<int>(std::lround((deg + 90.0F) / 60.0F));
    k = ((k % 6) + 6) % 6;
    return static_cast<HexDir>(k);
}

// Canonical world-space heading (radians) for a direction — the inverse of
// quantizeToHexDir's snapping. Direction k sits at (-90 + 60*k) degrees. Free
// movement converts a bot's chosen HexDir into the angle it steers toward.
[[nodiscard]] inline float dirAngle(HexDir d) {
    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0F;
    return (-90.0F + (60.0F * static_cast<float>(static_cast<int>(d)))) * kDegToRad;
}

// Wrap an angle difference into (-pi, pi], so steering always turns the short way.
[[nodiscard]] inline float wrapAngle(float a) {
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr float kTwoPi = 2.0F * kPi;
    a = std::fmod(a + kPi, kTwoPi);
    if (a < 0.0F) {
        a += kTwoPi;
    }
    return a - kPi;
}

[[nodiscard]] inline Vec2 unitFromAngle(float a) {
    return {std::cos(a), std::sin(a)};
}

// Round fractional axial coords to the nearest hex via cube rounding (reconcile
// the three cube axes by fixing the component with the largest rounding error).
[[nodiscard]] inline HexCoord hexRound(float qf, float rf) {
    const float x = qf;
    const float z = rf;
    const float y = -x - z;
    float rx = std::round(x);
    float ry = std::round(y);
    float rz = std::round(z);
    const float dx = std::abs(rx - x);
    const float dy = std::abs(ry - y);
    const float dz = std::abs(rz - z);
    if (dx > dy && dx > dz) {
        rx = -ry - rz;
    } else if (dy > dz) {
        ry = -rx - rz;
    } else {
        rz = -rx - ry;
    }
    return {static_cast<int>(rx), static_cast<int>(rz)};
}

// World-pixel center -> nearest flat-top axial cell. Inverse of axialToWorld; the
// hex a free-moving avatar currently sits over.
[[nodiscard]] inline HexCoord worldToAxial(Vec2 w, float size) {
    const float qf = w.x / (1.5F * size);
    const float rf = (w.y / (std::numbers::sqrt3_v<float> * size)) - (qf * 0.5F);
    return hexRound(qf, rf);
}

} // namespace og::hexanaut
