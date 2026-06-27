#include "games/hole/HoleWorld.hpp"

#include "games/hole/HoleConfig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace og::hole {
namespace {

namespace cfg = config;

[[nodiscard]] int clampDifficulty(int difficultyIndex) {
    return std::clamp(difficultyIndex, 0, 2);
}

[[nodiscard]] float sqr(float v) {
    return v * v;
}

[[nodiscard]] Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return a + ((b - a) * t);
}

[[nodiscard]] ObjectKind buildingKindForBlock(int ix, int iy) {
    if ((ix == 0 && iy == 3) || (ix == 3 && iy == 0)) {
        return ObjectKind::Tower;
    }
    if (((ix + iy) % 3) == 0) {
        return ObjectKind::Apartment;
    }
    if (((ix + iy) % 2) == 1) {
        return ObjectKind::Kiosk;
    }
    return ObjectKind::SmallBuilding;
}

} // namespace

HoleWorld::HoleWorld(int difficultyIndex, std::uint32_t seed)
    : difficultyIndex_(clampDifficulty(difficultyIndex)), rng_(seed) {
    player_.pos = {cfg::kWorldW * 0.5F, cfg::kWorldH * 0.5F};
    player_.score = 0.0F;
    refreshPlayerRadius();
    buildCity();
}

float HoleWorld::radiusForScore(float score, int difficultyIndex) {
    const cfg::DifficultyProfile& profile =
        cfg::kDifficultyProfiles.at(static_cast<std::size_t>(clampDifficulty(difficultyIndex)));
    const float s = std::max(0.0F, score);
    return std::clamp(profile.startRadius +
                          (cfg::kRadiusPerRootScore * std::sqrt(s * profile.growthScale)),
                      profile.startRadius, cfg::kMaxRadius);
}

int HoleWorld::playerScore() const {
    return static_cast<int>(std::lround(player_.score));
}

float HoleWorld::completionPercent() const {
    if (totalMass_ <= 0.0F) {
        return 100.0F;
    }
    return std::clamp((consumedMass_ / totalMass_) * 100.0F, 0.0F, 100.0F);
}

void HoleWorld::refreshPlayerRadius() {
    player_.radius = radiusForScore(player_.score, difficultyIndex_);
}

CityObject HoleWorld::makeObject(ObjectKind kind, Vec2 pos) const {
    const cfg::ObjectSpec& spec = cfg::specFor(kind);
    const cfg::DifficultyProfile& profile =
        cfg::kDifficultyProfiles.at(static_cast<std::size_t>(difficultyIndex_));
    CityObject object;
    object.kind = kind;
    object.pos = pos;
    object.mass = spec.mass;
    object.solidRadius = spec.solidRadius;
    object.consumeRadius = spec.consumeRadius;
    object.requiredRadius = spec.requiredRadius * profile.requirementScale;
    object.pathA = pos;
    object.pathB = pos;
    return object;
}

void HoleWorld::clearObjects() {
    objects_.clear();
    totalMass_ = 0.0F;
    consumedMass_ = 0.0F;
    totalObjects_ = 0;
    consumedObjects_ = 0;
}

void HoleWorld::addObject(const CityObject& object) {
    objects_.push_back(object);
    if (!object.consumed) {
        totalMass_ += object.mass;
        ++totalObjects_;
    } else {
        consumedMass_ += object.mass;
        ++totalObjects_;
        ++consumedObjects_;
    }
}

void HoleWorld::addStatic(ObjectKind kind, Vec2 pos) {
    addObject(makeObject(kind, pos));
}

void HoleWorld::addMobile(ObjectKind kind, Vec2 pathA, Vec2 pathB, float speed, float pathT) {
    CityObject object = makeObject(kind, lerp(pathA, pathB, std::clamp(pathT, 0.0F, 1.0F)));
    object.mobile = true;
    object.pathA = pathA;
    object.pathB = pathB;
    object.pathT = std::clamp(pathT, 0.0F, 1.0F);
    object.pathDir = 1.0F;
    object.speed = speed;
    addObject(object);
}

float HoleWorld::frand(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

void HoleWorld::buildCity() {
    objects_.reserve(220);

    const std::array<float, 4> blockCenters{450.0F, 1350.0F, 2250.0F, 3150.0F};
    for (int ix = 0; ix < 4; ++ix) {
        for (int iy = 0; iy < 4; ++iy) {
            const float cx = blockCenters.at(static_cast<std::size_t>(ix)) + frand(-35.0F, 35.0F);
            const float cy = blockCenters.at(static_cast<std::size_t>(iy)) + frand(-35.0F, 35.0F);
            addBlock(ix, iy, {cx, cy});
        }
    }

    addStarterRing();
    addRoadProps();
    addParkedVehicles();
    addMovingCityObjects();
}

void HoleWorld::addBlock(int ix, int iy, Vec2 center) {
    addStatic(buildingKindForBlock(ix, iy), center);
    addStatic(ObjectKind::Tree, center + Vec2{-250.0F, -250.0F});
    addStatic(ObjectKind::Tree, center + Vec2{250.0F, -250.0F});
    addStatic(ObjectKind::Tree, center + Vec2{-250.0F, 250.0F});
    addStatic(ObjectKind::Tree, center + Vec2{250.0F, 250.0F});
    addStatic(ObjectKind::Bench, center + Vec2{-215.0F, 0.0F});
    addStatic(ObjectKind::Bench, center + Vec2{215.0F, 0.0F});
    addStatic(ObjectKind::TrashCan, center + Vec2{0.0F, -250.0F});
    addStatic(ObjectKind::TrashCan, center + Vec2{0.0F, 250.0F});

    if (((ix + (iy * 2)) % 3) != 0) {
        addStatic(ObjectKind::Stand, center + Vec2{0.0F, -170.0F});
    }
    addStatic(ObjectKind::Cone, center + Vec2{-300.0F, -120.0F});
    addStatic(ObjectKind::Cone, center + Vec2{300.0F, 120.0F});
}

void HoleWorld::addStarterRing() {
    // Dense starter ring around the middle intersection so every difficulty can
    // begin by swallowing tiny objects and grow into the larger city.
    const Vec2 mid{cfg::kWorldW * 0.5F, cfg::kWorldH * 0.5F};
    for (int i = 0; i < 12; ++i) {
        const float a = (static_cast<float>(i) / 12.0F) * 2.0F * std::numbers::pi_v<float>;
        addStatic(ObjectKind::Cone, mid + Vec2{std::cos(a) * 150.0F, std::sin(a) * 150.0F});
    }
    addStatic(ObjectKind::TrashCan, mid + Vec2{-190.0F, 0.0F});
    addStatic(ObjectKind::TrashCan, mid + Vec2{190.0F, 0.0F});
    addStatic(ObjectKind::Pedestrian, mid + Vec2{0.0F, -210.0F});
    addStatic(ObjectKind::Pedestrian, mid + Vec2{0.0F, 210.0F});
}

void HoleWorld::addRoadProps() {
    const std::array<float, 3> roads{900.0F, 1800.0F, 2700.0F};
    for (const float r : roads) {
        for (int i = 0; i < 4; ++i) {
            const float offset = -330.0F + (static_cast<float>(i) * 220.0F);
            addStatic(ObjectKind::Cone, {r + offset, r - 130.0F});
            addStatic(ObjectKind::TrashCan, {r - 130.0F, r + offset});
        }
    }
}

void HoleWorld::addParkedVehicles() {
    // Parked vehicles along the road grid.
    for (int i = 0; i < 6; ++i) {
        const float x = 360.0F + (static_cast<float>(i) * 560.0F);
        addStatic((i % 3) == 0 ? ObjectKind::Van : ObjectKind::Car, {x, 1035.0F});
        addStatic((i % 2) == 0 ? ObjectKind::Car : ObjectKind::Van, {x, 2565.0F});
    }
    for (int i = 0; i < 6; ++i) {
        const float y = 360.0F + (static_cast<float>(i) * 560.0F);
        addStatic((i % 2) == 0 ? ObjectKind::Car : ObjectKind::Van, {1035.0F, y});
        addStatic((i % 3) == 0 ? ObjectKind::Van : ObjectKind::Car, {2565.0F, y});
    }
}

void HoleWorld::addMovingCityObjects() {
    // Moving traffic and walkers. The central cross is intentionally kept clear
    // at spawn so the player is not immediately blocked by a large vehicle.
    addMobile(ObjectKind::Car, {220.0F, 830.0F}, {3380.0F, 830.0F}, 185.0F, 0.12F);
    addMobile(ObjectKind::Van, {3380.0F, 970.0F}, {220.0F, 970.0F}, 150.0F, 0.42F);
    addMobile(ObjectKind::Car, {220.0F, 2630.0F}, {3380.0F, 2630.0F}, 170.0F, 0.66F);
    addMobile(ObjectKind::Van, {3380.0F, 2770.0F}, {220.0F, 2770.0F}, 145.0F, 0.24F);
    addMobile(ObjectKind::Car, {830.0F, 220.0F}, {830.0F, 3380.0F}, 175.0F, 0.32F);
    addMobile(ObjectKind::Van, {970.0F, 3380.0F}, {970.0F, 220.0F}, 145.0F, 0.72F);
    addMobile(ObjectKind::Car, {2630.0F, 220.0F}, {2630.0F, 3380.0F}, 165.0F, 0.52F);
    addMobile(ObjectKind::Van, {2770.0F, 3380.0F}, {2770.0F, 220.0F}, 150.0F, 0.08F);

    addMobile(ObjectKind::Pedestrian, {620.0F, 620.0F}, {620.0F, 780.0F}, 70.0F, 0.15F);
    addMobile(ObjectKind::Pedestrian, {1540.0F, 1240.0F}, {1700.0F, 1240.0F}, 60.0F, 0.55F);
    addMobile(ObjectKind::Pedestrian, {2380.0F, 2360.0F}, {2380.0F, 2540.0F}, 65.0F, 0.35F);
    addMobile(ObjectKind::Pedestrian, {3140.0F, 2860.0F}, {3320.0F, 2860.0F}, 70.0F, 0.75F);
}

void HoleWorld::step() {
    updateMobileObjects();
    integratePlayer();
    resolveConsumptionAndBlocking();
}

void HoleWorld::updateMobileObjects() {
    for (CityObject& object : objects_) {
        if (!object.mobile || object.consumed) {
            continue;
        }
        const float pathLen = length(object.pathB - object.pathA);
        if (pathLen <= 1.0F) {
            object.pos = object.pathA;
            continue;
        }
        object.pathT += object.pathDir * object.speed * cfg::kFixedDt / pathLen;
        if (object.pathT > 1.0F) {
            object.pathT = 2.0F - object.pathT;
            object.pathDir = -1.0F;
        } else if (object.pathT < 0.0F) {
            object.pathT = -object.pathT;
            object.pathDir = 1.0F;
        }
        object.pathT = std::clamp(object.pathT, 0.0F, 1.0F);
        object.pos = lerp(object.pathA, object.pathB, object.pathT);
    }
}

void HoleWorld::integratePlayer() {
    const cfg::DifficultyProfile& profile =
        cfg::kDifficultyProfiles.at(static_cast<std::size_t>(difficultyIndex_));
    Vec2 desiredVel{0.0F, 0.0F};
    if (input_.active) {
        const Vec2 toAim = input_.aimWorld - player_.pos;
        const float dist = length(toAim);
        if (dist > 4.0F) {
            const float speedScale = std::clamp(dist / 220.0F, 0.25F, 1.0F);
            desiredVel = normalize(toAim) * (profile.baseSpeed * speedScale);
        }
    }

    if (input_.active) {
        const float t = 1.0F - std::exp(-cfg::kSteerResponse * cfg::kFixedDt);
        player_.vel = player_.vel + ((desiredVel - player_.vel) * t);
    } else {
        const float damping = std::exp(-cfg::kFriction * cfg::kFixedDt);
        player_.vel = player_.vel * damping;
        if (lengthSq(player_.vel) < 1.0F) {
            player_.vel = {0.0F, 0.0F};
        }
    }

    const float maxSpeed = profile.baseSpeed * 1.08F;
    const float speed = length(player_.vel);
    if (speed > maxSpeed) {
        player_.vel = normalize(player_.vel) * maxSpeed;
    }
    player_.pos = player_.pos + (player_.vel * cfg::kFixedDt);
    clampPlayerToWorld();
}

void HoleWorld::resolveConsumptionAndBlocking() {
    for (CityObject& object : objects_) {
        if (object.consumed) {
            continue;
        }
        const Vec2 delta = player_.pos - object.pos;
        const float d2 = lengthSq(delta);
        if (player_.radius + 1.0e-3F >= object.requiredRadius) {
            const float reach = (player_.radius * cfg::kConsumptionReach) + object.consumeRadius;
            if (d2 <= sqr(reach)) {
                consume(object);
            }
            continue;
        }

        const float blockRadius = std::max(8.0F, player_.radius * cfg::kHoleBlockFactor) +
                                  object.solidRadius + cfg::kObjectPushPadding;
        if (d2 >= sqr(blockRadius)) {
            continue;
        }

        const float d = std::sqrt(std::max(d2, 0.0F));
        const Vec2 normal = d > 1.0e-4F ? delta / d : Vec2{1.0F, 0.0F};
        player_.pos = object.pos + (normal * blockRadius);
        const float into = dot(player_.vel, normal);
        if (into < 0.0F) {
            player_.vel = player_.vel - (normal * into);
        }
        clampPlayerToWorld();
    }
}

void HoleWorld::consume(CityObject& object) {
    if (object.consumed) {
        return;
    }
    object.consumed = true;
    consumedMass_ += object.mass;
    ++consumedObjects_;
    player_.score += object.mass;
    refreshPlayerRadius();
}

void HoleWorld::clampPlayerToWorld() {
    const float margin = std::max(18.0F, player_.radius * 0.35F);
    const float minX = margin;
    const float maxX = cfg::kWorldW - margin;
    const float minY = margin;
    const float maxY = cfg::kWorldH - margin;
    player_.pos.x = std::clamp(player_.pos.x, minX, maxX);
    player_.pos.y = std::clamp(player_.pos.y, minY, maxY);
    if ((player_.pos.x <= minX && player_.vel.x < 0.0F) ||
        (player_.pos.x >= maxX && player_.vel.x > 0.0F)) {
        player_.vel.x = 0.0F;
    }
    if ((player_.pos.y <= minY && player_.vel.y < 0.0F) ||
        (player_.pos.y >= maxY && player_.vel.y > 0.0F)) {
        player_.vel.y = 0.0F;
    }
}

} // namespace og::hole
