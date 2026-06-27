#pragma once

#include "games/hole/HoleConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace og::hole {

// Pure, SDL-free Hole simulation. The player and bot holes compete in a larger
// timed city: every participant can swallow city objects, larger holes can eat
// smaller holes, and bots respawn so the match stays populated. Deterministic
// for a given (difficulty, seed, input sequence), so all gameplay rules remain
// testable without rendering.
class HoleWorld {
public:
    HoleWorld(int difficultyIndex, std::uint32_t seed);

    void setPlayerInput(const PlayerInput& input) { input_ = input; }
    void step();

    [[nodiscard]] const HolePlayer& player() const { return holes_.front(); }
    [[nodiscard]] HolePlayer& playerRef() { return holes_.front(); }
    [[nodiscard]] const std::vector<HolePlayer>& holes() const { return holes_; }
    [[nodiscard]] HolePlayer& holeRef(std::size_t index) { return holes_.at(index); }
    [[nodiscard]] const std::vector<CityObject>& objects() const { return objects_; }
    [[nodiscard]] CityObject& objectRef(std::size_t index) { return objects_.at(index); }
    [[nodiscard]] float worldW() const { return profile_.worldW; }
    [[nodiscard]] float worldH() const { return profile_.worldH; }
    [[nodiscard]] int districtCount() const { return profile_.districtCount; }
    [[nodiscard]] int botCount() const { return profile_.botCount; }
    [[nodiscard]] int playerScore() const;
    [[nodiscard]] int playerRank() const;
    [[nodiscard]] float completionPercent() const;
    [[nodiscard]] bool completed() const { return consumedObjects_ == totalObjects_; }
    [[nodiscard]] bool playerAlive() const { return player().alive; }
    [[nodiscard]] bool timedOut() const { return timeRemaining_ <= 0.0F; }
    [[nodiscard]] bool finished() const { return !playerAlive() || timedOut() || completed(); }
    [[nodiscard]] float timeRemaining() const { return timeRemaining_; }
    [[nodiscard]] std::size_t remainingObjectCount() const {
        return totalObjects_ - consumedObjects_;
    }
    [[nodiscard]] std::size_t totalObjectCount() const { return totalObjects_; }

    [[nodiscard]] static float radiusForScore(float score, int difficultyIndex);

    // Test seams: let tests build tiny worlds without touching scene/rendering.
    void clearObjects();
    void clearBotsForTest();
    void addObject(const CityObject& object);
    void refreshHoleRadius(std::size_t index);
    void refreshPlayerRadius() { refreshHoleRadius(0); }

private:
    void buildCity();
    void spawnHoles();
    void spawnBot(std::size_t index);
    void addBlock(int ix, int iy, Vec2 center);
    void addStarterRing();
    void addRoadProps();
    void addParkedVehicles();
    void addMovingCityObjects();
    void addStatic(ObjectKind kind, Vec2 pos);
    void addMobile(ObjectKind kind, Vec2 pathA, Vec2 pathB, float speed, float pathT);
    [[nodiscard]] CityObject makeObject(ObjectKind kind, Vec2 pos) const;

    void updateMobileObjects();
    void updateBotBrains();
    void integrateHoles();
    void integrateHole(HolePlayer& hole, Vec2 aim, bool active, float baseSpeed);
    void resolveObjectInteractions();
    void resolveRivalEating();
    void updateRespawns();
    void consume(CityObject& object, HolePlayer& hole);
    void clampHoleToWorld(HolePlayer& hole) const;

    [[nodiscard]] Vec2 randomSpawnPosition(float clearance);
    [[nodiscard]] const CityObject* nearestEdibleObject(Vec2 from, float radius,
                                                        float maxDistance) const;
    [[nodiscard]] const HolePlayer* nearestThreat(const HolePlayer& self, float maxDistance) const;
    [[nodiscard]] const HolePlayer* nearestPrey(const HolePlayer& self, float maxDistance) const;
    [[nodiscard]] Vec2 inwardSteer(Vec2 pos) const;
    [[nodiscard]] float frand(float lo, float hi);

    int difficultyIndex_;
    config::DifficultyProfile profile_;
    std::mt19937 rng_;
    std::vector<HolePlayer> holes_;
    PlayerInput input_;
    std::vector<CityObject> objects_;
    float timeRemaining_ = config::kRoundSeconds;
    float totalMass_ = 0.0F;
    float consumedMass_ = 0.0F;
    std::size_t totalObjects_ = 0;
    std::size_t consumedObjects_ = 0;
};

} // namespace og::hole
