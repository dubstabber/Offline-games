#pragma once

#include "games/hole/HoleTypes.hpp"

#include <array>

// Tuning for the Hole simulation. This is pure data so the world and tests can
// share it without depending on SDL or the scene.
namespace og::hole::config {

inline constexpr float kFixedDt = 1.0F / 60.0F;
inline constexpr float kMaxAccumDt = 0.25F;
inline constexpr float kRoundSeconds = 120.0F;

inline constexpr float kSteerResponse = 8.0F;
inline constexpr float kFriction = 4.2F;
inline constexpr float kConsumptionReach = 0.72F;
inline constexpr float kHoleBlockFactor = 0.45F;
inline constexpr float kObjectPushPadding = 4.0F;
inline constexpr float kMaxRadius = 360.0F;
inline constexpr float kRadiusPerRootScore = 5.4F;

inline constexpr float kRivalEatRadiusFactor = 0.72F;
inline constexpr float kRivalEatSizeAdvantage = 1.15F;
inline constexpr float kRivalScoreBonus = 12.0F;
inline constexpr float kRivalScoreScale = 0.45F;
inline constexpr float kBotRespawnSeconds = 2.0F;
inline constexpr float kContactGraceSeconds = 1.5F;

struct DifficultyProfile {
    float worldW;
    float worldH;
    int districtCount;
    int botCount;
    float startRadius;
    float baseSpeed;
    float botSpeedScale;
    float growthScale;
    float requirementScale;
};

inline constexpr std::array<DifficultyProfile, 3> kDifficultyProfiles{{
    {.worldW = 4500.0F,
     .worldH = 4500.0F,
     .districtCount = 5,
     .botCount = 4,
     .startRadius = 36.0F,
     .baseSpeed = 400.0F,
     .botSpeedScale = 0.90F,
     .growthScale = 1.15F,
     .requirementScale = 0.92F},
    {.worldW = 5400.0F,
     .worldH = 5400.0F,
     .districtCount = 6,
     .botCount = 6,
     .startRadius = 32.0F,
     .baseSpeed = 370.0F,
     .botSpeedScale = 1.00F,
     .growthScale = 1.0F,
     .requirementScale = 1.0F},
    {.worldW = 6300.0F,
     .worldH = 6300.0F,
     .districtCount = 7,
     .botCount = 8,
     .startRadius = 28.0F,
     .baseSpeed = 345.0F,
     .botSpeedScale = 1.08F,
     .growthScale = 0.95F,
     .requirementScale = 1.08F},
}};

struct ObjectSpec {
    ObjectKind kind;
    float mass;
    float solidRadius;
    float consumeRadius;
    float requiredRadius;
};

inline constexpr std::array<ObjectSpec, 21> kObjectSpecs{{
    {.kind = ObjectKind::Cone,
     .mass = 1.0F,
     .solidRadius = 10.0F,
     .consumeRadius = 8.0F,
     .requiredRadius = 20.0F},
    {.kind = ObjectKind::Mailbox,
     .mass = 1.5F,
     .solidRadius = 11.0F,
     .consumeRadius = 9.0F,
     .requiredRadius = 24.0F},
    {.kind = ObjectKind::FireHydrant,
     .mass = 2.5F,
     .solidRadius = 13.0F,
     .consumeRadius = 10.0F,
     .requiredRadius = 32.0F},
    {.kind = ObjectKind::Pedestrian,
     .mass = 3.0F,
     .solidRadius = 12.0F,
     .consumeRadius = 10.0F,
     .requiredRadius = 30.0F},
    {.kind = ObjectKind::TrashCan,
     .mass = 2.0F,
     .solidRadius = 14.0F,
     .consumeRadius = 11.0F,
     .requiredRadius = 26.0F},
    {.kind = ObjectKind::Streetlight,
     .mass = 5.0F,
     .solidRadius = 18.0F,
     .consumeRadius = 14.0F,
     .requiredRadius = 40.0F},
    {.kind = ObjectKind::Bench,
     .mass = 4.0F,
     .solidRadius = 26.0F,
     .consumeRadius = 18.0F,
     .requiredRadius = 36.0F},
    {.kind = ObjectKind::Tree,
     .mass = 8.0F,
     .solidRadius = 34.0F,
     .consumeRadius = 24.0F,
     .requiredRadius = 48.0F},
    {.kind = ObjectKind::Motorcycle,
     .mass = 12.0F,
     .solidRadius = 34.0F,
     .consumeRadius = 26.0F,
     .requiredRadius = 62.0F},
    {.kind = ObjectKind::Stand,
     .mass = 15.0F,
     .solidRadius = 44.0F,
     .consumeRadius = 34.0F,
     .requiredRadius = 68.0F},
    {.kind = ObjectKind::Car,
     .mass = 25.0F,
     .solidRadius = 54.0F,
     .consumeRadius = 42.0F,
     .requiredRadius = 88.0F},
    {.kind = ObjectKind::Pickup,
     .mass = 32.0F,
     .solidRadius = 60.0F,
     .consumeRadius = 48.0F,
     .requiredRadius = 102.0F},
    {.kind = ObjectKind::Van,
     .mass = 40.0F,
     .solidRadius = 66.0F,
     .consumeRadius = 52.0F,
     .requiredRadius = 112.0F},
    {.kind = ObjectKind::Bus,
     .mass = 75.0F,
     .solidRadius = 92.0F,
     .consumeRadius = 72.0F,
     .requiredRadius = 150.0F},
    {.kind = ObjectKind::Fountain,
     .mass = 95.0F,
     .solidRadius = 92.0F,
     .consumeRadius = 72.0F,
     .requiredRadius = 165.0F},
    {.kind = ObjectKind::Kiosk,
     .mass = 60.0F,
     .solidRadius = 80.0F,
     .consumeRadius = 62.0F,
     .requiredRadius = 135.0F},
    {.kind = ObjectKind::SmallBuilding,
     .mass = 120.0F,
     .solidRadius = 125.0F,
     .consumeRadius = 96.0F,
     .requiredRadius = 180.0F},
    {.kind = ObjectKind::Office,
     .mass = 170.0F,
     .solidRadius = 145.0F,
     .consumeRadius = 112.0F,
     .requiredRadius = 210.0F},
    {.kind = ObjectKind::Apartment,
     .mass = 220.0F,
     .solidRadius = 170.0F,
     .consumeRadius = 130.0F,
     .requiredRadius = 235.0F},
    {.kind = ObjectKind::Tower,
     .mass = 360.0F,
     .solidRadius = 225.0F,
     .consumeRadius = 172.0F,
     .requiredRadius = 270.0F},
    {.kind = ObjectKind::Skyscraper,
     .mass = 520.0F,
     .solidRadius = 250.0F,
     .consumeRadius = 190.0F,
     .requiredRadius = 320.0F},
}};

[[nodiscard]] constexpr const ObjectSpec& specFor(ObjectKind kind) {
    for (const ObjectSpec& spec : kObjectSpecs) {
        if (spec.kind == kind) {
            return spec;
        }
    }
    return kObjectSpecs.front();
}

} // namespace og::hole::config
