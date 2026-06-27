#pragma once

#include <cmath>
#include <cstdint>

// Pure value types for Hole. The simulation treats every edible city object as a
// disc for movement/consumption, while the scene is free to draw richer shapes
// for each kind. No SDL or rendering types live here.
namespace og::hole {

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

[[nodiscard]] constexpr Vec2 operator/(Vec2 v, float s) {
    return {v.x / s, v.y / s};
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
    return len > 1.0e-6F ? v / len : Vec2{0.0F, 0.0F};
}

enum class ObjectKind : std::uint8_t {
    Cone,
    Pedestrian,
    TrashCan,
    Bench,
    Tree,
    Stand,
    Car,
    Van,
    Kiosk,
    SmallBuilding,
    Apartment,
    Tower,
};

struct CityObject {
    ObjectKind kind = ObjectKind::Cone;
    Vec2 pos;
    float mass = 1.0F;
    float solidRadius = 10.0F;
    float consumeRadius = 8.0F;
    float requiredRadius = 20.0F;
    bool consumed = false;

    bool mobile = false;
    Vec2 pathA;
    Vec2 pathB;
    float pathT = 0.0F;
    float pathDir = 1.0F;
    float speed = 0.0F;
};

struct PlayerInput {
    Vec2 aimWorld;
    bool active = false;
};

struct HolePlayer {
    Vec2 pos;
    Vec2 vel;
    float score = 0.0F;
    float radius = 0.0F;
};

} // namespace og::hole
