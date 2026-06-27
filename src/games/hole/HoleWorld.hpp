#pragma once

#include "games/hole/HoleConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace og::hole {

// Pure, SDL-free Hole simulation. The player steers a growing hole around a
// finite handmade city; small objects are consumed first, bigger objects block
// movement until the hole has grown enough to swallow them. Deterministic for a
// given (difficulty, seed, input sequence), so all gameplay rules are testable
// without rendering.
class HoleWorld {
public:
    HoleWorld(int difficultyIndex, std::uint32_t seed);

    void setPlayerInput(const PlayerInput& input) { input_ = input; }
    void step();

    [[nodiscard]] const HolePlayer& player() const { return player_; }
    [[nodiscard]] HolePlayer& playerRef() { return player_; }
    [[nodiscard]] const std::vector<CityObject>& objects() const { return objects_; }
    [[nodiscard]] CityObject& objectRef(std::size_t index) { return objects_.at(index); }
    [[nodiscard]] static float worldW() { return config::kWorldW; }
    [[nodiscard]] static float worldH() { return config::kWorldH; }
    [[nodiscard]] int playerScore() const;
    [[nodiscard]] float completionPercent() const;
    [[nodiscard]] bool completed() const { return consumedObjects_ == totalObjects_; }
    [[nodiscard]] std::size_t remainingObjectCount() const {
        return totalObjects_ - consumedObjects_;
    }
    [[nodiscard]] std::size_t totalObjectCount() const { return totalObjects_; }

    [[nodiscard]] static float radiusForScore(float score, int difficultyIndex);

    // Test seams: let tests build tiny worlds without touching scene/rendering.
    void clearObjects();
    void addObject(const CityObject& object);
    void refreshPlayerRadius();

private:
    void buildCity();
    void addBlock(int ix, int iy, Vec2 center);
    void addStarterRing();
    void addRoadProps();
    void addParkedVehicles();
    void addMovingCityObjects();
    void addStatic(ObjectKind kind, Vec2 pos);
    void addMobile(ObjectKind kind, Vec2 pathA, Vec2 pathB, float speed, float pathT);
    [[nodiscard]] CityObject makeObject(ObjectKind kind, Vec2 pos) const;

    void updateMobileObjects();
    void integratePlayer();
    void resolveConsumptionAndBlocking();
    void consume(CityObject& object);
    void clampPlayerToWorld();

    [[nodiscard]] float frand(float lo, float hi);

    int difficultyIndex_;
    std::mt19937 rng_;
    HolePlayer player_;
    PlayerInput input_;
    std::vector<CityObject> objects_;
    float totalMass_ = 0.0F;
    float consumedMass_ = 0.0F;
    std::size_t totalObjects_ = 0;
    std::size_t consumedObjects_ = 0;
};

} // namespace og::hole
