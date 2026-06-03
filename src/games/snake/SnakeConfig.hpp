#pragma once

#include "games/snake/SnakeTypes.hpp"

#include <array>
#include <numbers>
#include <string_view>

// Tuning for the Snake simulation. Pure data (no SDL/Color) so it can be pulled
// into the SDL-free SnakeWorld and the unit tests. The numbers are rescaled from
// the original JindoBlu "Snakes" binary (SnakesSimulatorConfig.asset): the
// *ratios* (boost ~2x, turn ~ a few rad/s, food packs 10/20/40/100 by weight,
// death-loot fractions, a square arena) are preserved, the absolute units are
// scaled to our 720x1440 logical canvas.
namespace og::snake::config {

inline constexpr float kPi = std::numbers::pi_v<float>;

// ---- World / timestep -------------------------------------------------------
inline constexpr float kWorldSize = 5000.0F;    // square arena edge (units)
inline constexpr float kFixedDt = 1.0F / 60.0F; // deterministic sim sub-step
inline constexpr float kMaxAccumDt = 0.25F;     // clamp big frame hitches

// ---- Movement ---------------------------------------------------------------
inline constexpr float kBaseSpeed = 280.0F; // units/s (original 14.5, rescaled)
inline constexpr float kBoostMultiplier = 1.9F;
inline constexpr float kMaxTurnRate = 5.0F; // rad/s (original 6, eased for touch)
inline constexpr float kInvulnSeconds = 3.5F;

// ---- Body geometry (length & radius are pure functions of score) ------------
inline constexpr int kBaseSegments = 10;
inline constexpr int kMaxSegments = 280; // original MaxSegmentCount
inline constexpr float kSegmentsPerSqrtScore = 1.6F;
inline constexpr float kBaseRadius = 22.0F;
inline constexpr float kRadiusCap = 64.0F;
inline constexpr float kRadiusGrowth = 0.55F;
inline constexpr float kRadiusScoreScale = 1.0F / 60.0F;
inline constexpr float kSpacingFactor = 0.45F; // spacing = radius * factor (overlap)

// ---- Boost economy ----------------------------------------------------------
inline constexpr float kBoostMassBleedPerSec = 8.0F;
inline constexpr float kBoostMinScore = 20.0F; // can't boost below this
inline constexpr float kBoostDropMass = 6.0F;  // mass per orb dropped while boosting

// ---- Food -------------------------------------------------------------------
inline constexpr int kFoodTargetCount = 300;
inline constexpr int kFoodMaxSpawnPerStep = 6;
inline constexpr int kFoodColorCount = 10;
inline constexpr float kFoodBaseRadius = 7.0F;
inline constexpr float kFoodRadiusPerMass = 0.7F;
inline constexpr float kFoodRadiusCap = 16.0F;

struct FoodPack {
    float mass;
    float weight;
};
// Original weighted packs {10:w10, 20:w2, 40:w1, 100:w0.3}, masses /10 for scale.
inline constexpr std::array<FoodPack, 4> kFoodPacks{{
    {.mass = 1.0F, .weight = 10.0F},
    {.mass = 2.0F, .weight = 2.0F},
    {.mass = 4.0F, .weight = 1.0F},
    {.mass = 10.0F, .weight = 0.3F},
}};

// ---- Death loot: fraction of a dead snake's score dropped as food -----------
// Indexed by difficulty 0/1/2 (Easy/Medium/Hard); original DeathLootByDifficulty.
inline constexpr std::array<float, 3> kDeathLoot{0.6F, 0.45F, 0.3F};
inline constexpr int kMaxDeathOrbs = 36;
inline constexpr float kDeathOrbMinMass = 1.0F;

// ---- Bots -------------------------------------------------------------------
inline constexpr int kBotCount = 20;
inline constexpr int kSnakeGradientCount = 10;
inline constexpr float kBotLookAhead = 320.0F; // body-avoidance probe distance
inline constexpr float kBotEdgeMargin = 360.0F;

// One "personality" preset per difficulty. Easy bots are cautious and passive;
// Hard bots avoid death well, hunt aggressively, and turn/boost more.
[[nodiscard]] constexpr BotConfig botPresetFor(int difficultyIndex) {
    switch (difficultyIndex) {
    case 0:
        return {.crashAvoidance = 0.6F,
                .aggressiveness = 0.10F,
                .maxTurn = 3.8F,
                .boostBias = 0.05F,
                .foodSeekRadius = 900.0F};
    case 2:
        return {.crashAvoidance = 1.0F,
                .aggressiveness = 0.85F,
                .maxTurn = 5.6F,
                .boostBias = 0.35F,
                .foodSeekRadius = 1400.0F};
    default:
        return {.crashAvoidance = 0.85F,
                .aggressiveness = 0.40F,
                .maxTurn = 4.8F,
                .boostBias = 0.15F,
                .foodSeekRadius = 1100.0F};
    }
}

// Bot/player display names. A representative subset of the original's pool; the
// full 500+ list lives in the extracted PlayerProfiles.asset. The names visible
// in the demo screenshot are kept up front.
inline constexpr auto kNamePool = std::to_array<std::string_view>(
    {"Evan",       "Lily",       "Pablo",       "Alberto",    "SpinFlow",   "SlinkRush",
     "Jellybyte",  "FizzleWorm", "FrostTail",   "LoopBug",    "Rachel",     "Isaac",
     "Ada",        "Marie",      "Rosa",        "Liam",       "Emma",       "Noah",
     "Ella",       "Jack",       "Lucy",        "Owen",       "Grace",      "Maya",
     "Ruby",       "Zoe",        "Adam",        "Mia",        "Max",        "Nora",
     "Finn",       "Rose",       "Leo",         "Kate",       "Jude",       "Iris",
     "Aiden",      "Clara",      "Elara",       "Sienna",     "Rafael",     "Ivy",
     "Tobias",     "Amira",      "Felix",       "Lila",       "Mira",       "Cedric",
     "Rowan",      "Zara",       "Freya",       "Nova",       "Selene",     "Hazel",
     "Orion",      "Cora",       "Thea",        "Dahlia",     "Isla",       "Aurelia",
     "Maren",      "Esme",       "Juniper",     "Lyra",       "GlowMite",   "SerpentByte",
     "Wrigglex",   "CoilMaster", "SlipStream",  "NeonNoodle", "VenomPulse", "SnackWorm",
     "Slinkster",  "Spiralix",   "ByteBoa",     "Zoomtail",   "Slithra",    "ZapSnake",
     "Twistify",   "Creeptor",   "PulseTail",   "DashVine",   "GlimSnake",  "TurboTwine",
     "OrbSnake",   "Flickline",  "ChromaCreep", "Loopra",     "SlimeRush",  "Serpino",
     "JellyVine",  "DriftTail",  "Venoro",      "Trailix",    "LumenWorm",  "FangLoop",
     "Cobrax",     "Slideglow",  "SwirlVine",   "QuiverTail", "Noodlar",    "GlimmerBug",
     "BuzzTail",   "Gloopy",     "ThornSpine",  "ShineTail",  "VortexWorm", "GlowTail",
     "NeonSpine",  "Loopworm",   "SlurpTail",   "Spincoil",   "TwineBot",   "RazorTail",
     "FlareSnake", "NoodlePop",  "Venomite",    "FlowSnake",  "ZippySnake", "MagmaWorm"});

} // namespace og::snake::config
