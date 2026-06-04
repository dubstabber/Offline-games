#pragma once

#include "games/hexanaut/BotController.hpp"

// Tuning constants for Hexanaut: the fixed-timestep cadence (shared with the
// Snake pattern), the fake-3D projection knobs, and the camera. Pure data, no
// SDL — the projection numbers are consumed by HexanautScene at draw time.
namespace og::hexanaut::config {

// Fixed simulation step (deterministic), with a clamp on big frame hitches —
// same discipline as SnakeConfig.
inline constexpr float kFixedDt = 1.0F / 60.0F;
inline constexpr float kMaxAccumDt = 0.25F;

// ---- Fake-3D projection -----------------------------------------------------
inline constexpr float kHexSize = 26.0F;        // world units, center to corner
inline constexpr float kSquash = 0.58F;         // vertical foreshorten for the tilt
inline constexpr float kPrismLiftFactor = 0.5F; // prism height as a fraction of kHexSize
inline constexpr float kPrismLift = kHexSize * kPrismLiftFactor;
inline constexpr float kGroundInset = 0.9F; // flat ground hexes drawn slightly small -> grid gaps

inline constexpr float kTrailLiftFactor = 1.3F; // trails ride a bit above territory
inline constexpr float kTrailLift = kPrismLift * kTrailLiftFactor;

// ---- Camera -----------------------------------------------------------------
inline constexpr float kBaseZoom = 1.0F;
inline constexpr float kFollowRate = 10.0F;

// ---- Spawn ------------------------------------------------------------------
inline constexpr int kHomeRadius = 2; // starting territory radius (hexes)

// ---- Power-ups --------------------------------------------------------------
inline constexpr float kSpeedFactor = 0.6F;   // multiplies stepInterval (lower = faster)
inline constexpr float kSpeedDuration = 5.0F; // seconds
inline constexpr float kVisionDuration = 7.0F;
inline constexpr float kVisionZoom = 0.72F; // camera zoom while Vision is active (zoomed out)

// ---- Per-difficulty parameters ----------------------------------------------
// Lower stepInterval = faster. Harder = bigger map, more & faster bots, more
// power-ups. powerupInterval == 0 disables power-up spawns for that difficulty.
struct DifficultyParams {
    int gridW;
    int gridH;
    int botCount;
    BotSkill botSkill;
    float playerStepInterval; // seconds per hex
    float botStepInterval;
    float powerupInterval; // seconds between spawn attempts (0 = none)
    int maxPowerups;
};

[[nodiscard]] constexpr DifficultyParams paramsFor(int difficultyIndex) {
    switch (difficultyIndex) {
    case 0:
        return {.gridW = 56,
                .gridH = 56,
                .botCount = 4,
                .botSkill = BotSkill::Basic,
                .playerStepInterval = 0.14F,
                .botStepInterval = 0.17F,
                .powerupInterval = 0.0F,
                .maxPowerups = 0};
    case 2:
        return {.gridW = 72,
                .gridH = 72,
                .botCount = 10,
                .botSkill = BotSkill::Smart, // falls back to Basic until Phase E
                .playerStepInterval = 0.10F,
                .botStepInterval = 0.11F,
                .powerupInterval = 6.0F,
                .maxPowerups = 4};
    default:
        return {.gridW = 64,
                .gridH = 64,
                .botCount = 6,
                .botSkill = BotSkill::Basic,
                .playerStepInterval = 0.12F,
                .botStepInterval = 0.14F,
                .powerupInterval = 9.0F,
                .maxPowerups = 3};
    }
}

} // namespace og::hexanaut::config
