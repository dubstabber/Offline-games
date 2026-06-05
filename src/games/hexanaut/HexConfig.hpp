#pragma once

#include "games/hexanaut/BotController.hpp"

#include <numbers>

// Tuning constants for Hexanaut: the fixed-timestep cadence (shared with the
// Snake pattern), the fake-3D projection knobs, and the camera. Pure data, no
// SDL — the projection numbers are consumed by HexanautScene at draw time.
namespace og::hexanaut::config {

// Fixed simulation step (deterministic), with a clamp on big frame hitches —
// same discipline as SnakeConfig.
inline constexpr float kFixedDt = 1.0F / 60.0F;
inline constexpr float kMaxAccumDt = 0.25F;

// ---- Fake-3D projection -----------------------------------------------------
inline constexpr float kHexSize = 26.0F; // world units, center to corner
// Distance between adjacent flat-top hex centers (all 6 neighbors are sqrt3*size
// apart). Free movement converts a per-hex stepInterval into a world speed:
// speed = kHexSpacing / stepInterval, so an avatar crosses one hex in the same
// time the old discrete stepping did.
inline constexpr float kHexSpacing = std::numbers::sqrt3_v<float> * kHexSize;
inline constexpr float kSquash = 1.0F; // 1.0 = flat top-down (no vertical foreshorten)
// Tiles still get a shallow extrusion so owned land reads as a raised tile, but
// with the flat (un-tilted) projection the lift is kept small: a tall pop would
// look detached, floating above the straight-down ground. Tune in lockstep with
// kSquash — these two knobs are the whole "how 3D does it look" dial.
inline constexpr float kPrismLiftFactor = 0.18F; // prism height as a fraction of kHexSize
inline constexpr float kPrismLift = kHexSize * kPrismLiftFactor;
inline constexpr float kGroundInset = 0.93F; // flat ground hexes drawn slightly small -> grid gaps

inline constexpr float kTrailLiftFactor = 1.3F; // trails ride a bit above territory
inline constexpr float kTrailLift = kPrismLift * kTrailLiftFactor;

// The raised, glossy "rope" tube that trails behind a player while it is outside
// its territory (on top of the flat outlined trail cells): how high it floats
// above the ground and its radius as a fraction of kHexSize.
inline constexpr float kTrailRopeLift = kPrismLift;
inline constexpr float kTrailRopeRadius = 0.40F;

// ---- Free movement ----------------------------------------------------------
// Max heading change (radians/sec) as the avatar curves toward your finger. High
// enough for tight loops, low enough that it can't snap a 180° onto its own trail
// — a ~180° about-face takes roughly the time to cross two hexes.
inline constexpr float kTurnRate = 10.0F;

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

// ---- Shooter item -----------------------------------------------------------
// A persistent map item that, while it lies inside a player's territory, fires
// lasers that capture the nearest un-owned/enemy cell one at a time. The closer
// the frontier, the faster it shoots; past kShooterRange hexes it does nothing,
// so each shooter blooms a bounded disc of land around itself, slowing as it
// grows. Owned by whoever owns its cell, so it can be stolen by recapture.
inline constexpr int kShooterRange = 6;               // max hex distance it can reach
inline constexpr float kShooterFastInterval = 0.14F;  // s/capture when the target is adjacent
inline constexpr float kShooterSlowInterval = 0.95F;  // s/capture at the edge of range
inline constexpr float kShooterLaserLife = 0.4F;      // seconds a fired laser bolt takes to fade out

// Seconds between captures for a target `dist` hexes away (lerps fast->slow over
// the reach; clamped at the ends). Pure data so the sim stays SDL-free.
[[nodiscard]] constexpr float shooterShotInterval(int dist) {
    if (dist <= 1) {
        return kShooterFastInterval;
    }
    if (dist >= kShooterRange) {
        return kShooterSlowInterval;
    }
    const float t = static_cast<float>(dist - 1) / static_cast<float>(kShooterRange - 1);
    return kShooterFastInterval + ((kShooterSlowInterval - kShooterFastInterval) * t);
}

// ---- Slowing totem item -----------------------------------------------------
// A static item that, while owned, blankets a hex radius around it in snow and
// slows every OTHER avatar standing in that field (the owner is immune).
inline constexpr int kSlowRadius = 4;       // hex radius of the slowing field
inline constexpr float kSlowFactor = 1.85F; // multiplies stepInterval inside (>1 = slower)

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
    float powerupInterval; // seconds between Speed/Vision spawn attempts (0 = none)
    int maxPowerups;
    int shooterCount;   // static Shooter items placed once at game start (never respawn)
    int slowTotemCount; // static SlowTotem items placed once at game start
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
                .maxPowerups = 0,
                .shooterCount = 4,
                .slowTotemCount = 3};
    case 2:
        return {.gridW = 72,
                .gridH = 72,
                .botCount = 10,
                .botSkill = BotSkill::Smart, // falls back to Basic until Phase E
                .playerStepInterval = 0.10F,
                .botStepInterval = 0.11F,
                .powerupInterval = 6.0F,
                .maxPowerups = 4,
                .shooterCount = 8,
                .slowTotemCount = 5};
    default:
        return {.gridW = 64,
                .gridH = 64,
                .botCount = 6,
                .botSkill = BotSkill::Basic,
                .playerStepInterval = 0.12F,
                .botStepInterval = 0.14F,
                .powerupInterval = 9.0F,
                .maxPowerups = 3,
                .shooterCount = 6,
                .slowTotemCount = 4};
    }
}

} // namespace og::hexanaut::config
