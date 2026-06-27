#include "games/hole/HoleWorld.hpp"

#include "games/hole/HoleConfig.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

[[nodiscard]] float cellSize(const cfg::DifficultyProfile& profile) {
    return profile.worldW / static_cast<float>(profile.districtCount);
}

[[nodiscard]] float roadAt(const cfg::DifficultyProfile& profile, int index) {
    return cellSize(profile) * static_cast<float>(index);
}

[[nodiscard]] Vec2 spawnCenter(const cfg::DifficultyProfile& profile) {
    const float road = roadAt(profile, profile.districtCount / 2);
    return {road, road};
}

[[nodiscard]] ObjectKind buildingKindForBlock(int ix, int iy, int districts) {
    if (districts >= 7 && ((ix * 7) + (iy * 3)) % 11 == 0) {
        return ObjectKind::Skyscraper;
    }
    if ((ix == 0 && iy == districts - 1) || (ix == districts - 1 && iy == 0) ||
        ((ix + iy + districts) % 8 == 0)) {
        return ObjectKind::Tower;
    }
    if (((ix * 5) + iy) % 5 == 0) {
        return ObjectKind::Office;
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
    : difficultyIndex_(clampDifficulty(difficultyIndex)),
      profile_(cfg::kDifficultyProfiles.at(static_cast<std::size_t>(difficultyIndex_))),
      rng_(seed) {
    buildCity();
    spawnHoles();
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
    return static_cast<int>(std::lround(player().score));
}

int HoleWorld::playerRank() const {
    int rank = 1;
    const float score = player().score;
    for (std::size_t i = 1; i < holes_.size(); ++i) {
        const HolePlayer& other = holes_.at(i);
        if (other.score > score) {
            ++rank;
        }
    }
    return rank;
}

float HoleWorld::completionPercent() const {
    if (totalMass_ <= 0.0F) {
        return 100.0F;
    }
    return std::clamp((consumedMass_ / totalMass_) * 100.0F, 0.0F, 100.0F);
}

void HoleWorld::refreshHoleRadius(std::size_t index) {
    HolePlayer& hole = holes_.at(index);
    hole.radius = radiusForScore(hole.score, difficultyIndex_);
}

CityObject HoleWorld::makeObject(ObjectKind kind, Vec2 pos) const {
    const cfg::ObjectSpec& spec = cfg::specFor(kind);
    CityObject object;
    object.kind = kind;
    object.pos = pos;
    object.mass = spec.mass;
    object.solidRadius = spec.solidRadius;
    object.consumeRadius = spec.consumeRadius;
    object.requiredRadius = spec.requiredRadius * profile_.requirementScale;
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

void HoleWorld::clearBotsForTest() {
    holes_.resize(1);
    HolePlayer& p = holes_.front();
    p.isBot = false;
    p.alive = true;
    p.respawnTimer = 0.0F;
    p.graceTimer = 0.0F;
    p.colorIndex = 0;
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
    const int districts = profile_.districtCount;
    const auto districtCount = static_cast<std::size_t>(districts);
    objects_.reserve((districtCount * districtCount * std::size_t{17}) +
                     (districtCount * std::size_t{42}));

    const float cell = cellSize(profile_);
    for (int ix = 0; ix < districts; ++ix) {
        for (int iy = 0; iy < districts; ++iy) {
            const float cx = ((static_cast<float>(ix) + 0.5F) * cell) + frand(-38.0F, 38.0F);
            const float cy = ((static_cast<float>(iy) + 0.5F) * cell) + frand(-38.0F, 38.0F);
            addBlock(ix, iy, {cx, cy});
        }
    }

    addStarterRing();
    addRoadProps();
    addParkedVehicles();
    addMovingCityObjects();
}

void HoleWorld::spawnHoles() {
    holes_.clear();
    holes_.resize(static_cast<std::size_t>(profile_.botCount) + std::size_t{1});

    HolePlayer& p = holes_.front();
    p.pos = spawnCenter(profile_);
    p.vel = {0.0F, 0.0F};
    p.score = 0.0F;
    p.alive = true;
    p.isBot = false;
    p.respawnTimer = 0.0F;
    p.graceTimer = cfg::kContactGraceSeconds;
    p.botAim = p.pos;
    p.colorIndex = 0;
    refreshHoleRadius(0);

    for (std::size_t i = 1; i < holes_.size(); ++i) {
        HolePlayer& bot = holes_.at(i);
        bot.isBot = true;
        bot.colorIndex = static_cast<std::uint8_t>(i);
        bot.alive = false;
        spawnBot(i);
    }
}

void HoleWorld::spawnBot(std::size_t index) {
    HolePlayer& bot = holes_.at(index);
    bot.pos = randomSpawnPosition(profile_.startRadius * 3.0F);
    bot.vel = {0.0F, 0.0F};
    bot.score = 0.0F;
    bot.alive = true;
    bot.isBot = true;
    bot.respawnTimer = 0.0F;
    bot.graceTimer = cfg::kContactGraceSeconds;
    bot.botAim = bot.pos;
    refreshHoleRadius(index);
}

void HoleWorld::addBlock(int ix, int iy, Vec2 center) {
    const int districts = profile_.districtCount;
    addStatic(buildingKindForBlock(ix, iy, districts), center);
    addStatic(ObjectKind::Tree, center + Vec2{-250.0F, -250.0F});
    addStatic(ObjectKind::Tree, center + Vec2{250.0F, -250.0F});
    addStatic(ObjectKind::Tree, center + Vec2{-250.0F, 250.0F});
    addStatic(ObjectKind::Tree, center + Vec2{250.0F, 250.0F});
    addStatic(ObjectKind::Bench, center + Vec2{-220.0F, 0.0F});
    addStatic(ObjectKind::Bench, center + Vec2{220.0F, 0.0F});
    addStatic(ObjectKind::TrashCan, center + Vec2{0.0F, -250.0F});
    addStatic(ObjectKind::TrashCan, center + Vec2{0.0F, 250.0F});
    addStatic(ObjectKind::Mailbox, center + Vec2{-285.0F, 55.0F});
    addStatic(ObjectKind::FireHydrant, center + Vec2{285.0F, -55.0F});

    if (((ix + (iy * 2)) % 3) != 0) {
        addStatic(ObjectKind::Stand, center + Vec2{0.0F, -170.0F});
    }
    if (((ix * 3) + iy) % 5 == 0) {
        addStatic(ObjectKind::Fountain, center + Vec2{0.0F, 205.0F});
    } else {
        addStatic(ObjectKind::Streetlight, center + Vec2{0.0F, 205.0F});
    }
    addStatic(ObjectKind::Cone, center + Vec2{-305.0F, -120.0F});
    addStatic(ObjectKind::Cone, center + Vec2{305.0F, 120.0F});
}

void HoleWorld::addStarterRing() {
    // Dense starter ring around the initial intersection so every difficulty can
    // begin by swallowing tiny objects and grow into the larger city.
    const Vec2 mid = spawnCenter(profile_);
    for (int i = 0; i < 16; ++i) {
        const float a = (static_cast<float>(i) / 16.0F) * 2.0F * std::numbers::pi_v<float>;
        addStatic(ObjectKind::Cone, mid + Vec2{std::cos(a) * 150.0F, std::sin(a) * 150.0F});
    }
    addStatic(ObjectKind::Mailbox, mid + Vec2{-205.0F, -70.0F});
    addStatic(ObjectKind::Mailbox, mid + Vec2{205.0F, 70.0F});
    addStatic(ObjectKind::TrashCan, mid + Vec2{-205.0F, 70.0F});
    addStatic(ObjectKind::TrashCan, mid + Vec2{205.0F, -70.0F});
    addStatic(ObjectKind::Pedestrian, mid + Vec2{0.0F, -215.0F});
    addStatic(ObjectKind::Pedestrian, mid + Vec2{0.0F, 215.0F});
}

void HoleWorld::addRoadProps() {
    const int districts = profile_.districtCount;
    const float cell = cellSize(profile_);
    for (int ri = 1; ri < districts; ++ri) {
        const float road = roadAt(profile_, ri);
        for (int si = 0; si < districts; ++si) {
            const float center = (static_cast<float>(si) + 0.5F) * cell;
            const float offset = ((si % 2) == 0) ? -110.0F : 110.0F;
            addStatic(ObjectKind::Cone, {center + offset, road - 132.0F});
            addStatic(ObjectKind::FireHydrant, {center - offset, road + 132.0F});
            addStatic(ObjectKind::TrashCan, {road - 132.0F, center + offset});
            addStatic(ObjectKind::Mailbox, {road + 132.0F, center - offset});
            if (((ri + si) % 2) == 0) {
                addStatic(ObjectKind::Streetlight, {center, road + 156.0F});
                addStatic(ObjectKind::Streetlight, {road - 156.0F, center});
            }
        }
    }
}

void HoleWorld::addParkedVehicles() {
    const int districts = profile_.districtCount;
    const float cell = cellSize(profile_);
    for (int ri = 1; ri < districts; ++ri) {
        const float road = roadAt(profile_, ri);
        for (int si = 0; si < districts; ++si) {
            const float center = (static_cast<float>(si) + 0.5F) * cell;
            const int selector = (ri + (si * 2)) % 5;
            ObjectKind vehicle = ObjectKind::Car;
            if (selector == 0) {
                vehicle = ObjectKind::Motorcycle;
            } else if (selector == 2) {
                vehicle = ObjectKind::Pickup;
            } else if (selector == 4) {
                vehicle = ObjectKind::Van;
            }
            addStatic(vehicle, {center, road + (((ri + si) % 2) == 0 ? -158.0F : 158.0F)});

            const ObjectKind crossVehicle =
                ((selector + 1) % 4) == 0 ? ObjectKind::Van : ObjectKind::Car;
            addStatic(crossVehicle, {road + (((ri + si) % 2) == 0 ? 158.0F : -158.0F), center});
        }
    }
}

void HoleWorld::addMovingCityObjects() {
    const int districts = profile_.districtCount;
    for (int ri = 1; ri < districts; ++ri) {
        const float road = roadAt(profile_, ri);
        const float t = static_cast<float>((ri * 17) % 100) / 100.0F;
        const ObjectKind a = (ri % 3) == 0 ? ObjectKind::Bus : ObjectKind::Car;
        const ObjectKind b = (ri % 2) == 0 ? ObjectKind::Pickup : ObjectKind::Van;
        addMobile(a, {220.0F, road - 70.0F}, {worldW() - 220.0F, road - 70.0F},
                  170.0F + (static_cast<float>(ri) * 7.0F), t);
        addMobile(b, {worldW() - 220.0F, road + 72.0F}, {220.0F, road + 72.0F},
                  145.0F + (static_cast<float>(ri) * 6.0F), 1.0F - t);
        addMobile((ri % 3) == 1 ? ObjectKind::Bus : ObjectKind::Car, {road - 70.0F, 220.0F},
                  {road - 70.0F, worldH() - 220.0F}, 158.0F + (static_cast<float>(ri) * 5.0F),
                  0.25F + (t * 0.5F));
        addMobile(ObjectKind::Van, {road + 72.0F, worldH() - 220.0F}, {road + 72.0F, 220.0F},
                  142.0F + (static_cast<float>(ri) * 5.0F), 0.75F - (t * 0.5F));
    }

    const float cell = cellSize(profile_);
    for (int i = 0; i < districts; ++i) {
        const float x = (static_cast<float>(i) + 0.5F) * cell;
        const float y = (static_cast<float>((i * 2) % districts) + 0.5F) * cell;
        addMobile(ObjectKind::Pedestrian, {x - 90.0F, y - 70.0F}, {x + 90.0F, y + 70.0F},
                  55.0F + static_cast<float>(i * 3), 0.18F + (0.07F * static_cast<float>(i % 5)));
    }
}

void HoleWorld::step() {
    if (!playerAlive() || timedOut()) {
        return;
    }
    timeRemaining_ = std::max(0.0F, timeRemaining_ - cfg::kFixedDt);
    updateRespawns();
    updateMobileObjects();
    updateBotBrains();
    integrateHoles();
    resolveObjectInteractions();
    resolveRivalEating();
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

void HoleWorld::updateBotBrains() {
    for (std::size_t i = 1; i < holes_.size(); ++i) {
        HolePlayer& bot = holes_.at(i);
        if (!bot.alive) {
            continue;
        }
        Vec2 dir = lengthSq(bot.vel) > 1.0F ? normalize(bot.vel)
                                            : normalize(spawnCenter(profile_) - bot.pos);

        if (const HolePlayer* threat = nearestThreat(bot, 760.0F); threat != nullptr) {
            dir = dir + (normalize(bot.pos - threat->pos) * 3.8F);
        } else if (const HolePlayer* prey = nearestPrey(bot, 920.0F); prey != nullptr) {
            dir = dir + (normalize(prey->pos - bot.pos) * 2.6F);
        }

        if (const CityObject* edible = nearestEdibleObject(bot.pos, bot.radius, 1250.0F);
            edible != nullptr) {
            dir = dir + (normalize(edible->pos - bot.pos) * 2.4F);
        }

        dir = dir + (inwardSteer(bot.pos) * 2.5F);
        if (lengthSq(dir) < 1.0e-4F) {
            dir = normalize(spawnCenter(profile_) - bot.pos);
        }
        bot.botAim = bot.pos + (normalize(dir) * 700.0F);
    }
}

void HoleWorld::integrateHoles() {
    if (player().alive) {
        integrateHole(holes_.front(), input_.aimWorld, input_.active, profile_.baseSpeed);
    }
    for (std::size_t i = 1; i < holes_.size(); ++i) {
        HolePlayer& bot = holes_.at(i);
        if (bot.alive) {
            integrateHole(bot, bot.botAim, true, profile_.baseSpeed * profile_.botSpeedScale);
        }
    }
}

void HoleWorld::integrateHole(HolePlayer& hole, Vec2 aim, bool active, float baseSpeed) {
    Vec2 desiredVel{0.0F, 0.0F};
    if (active) {
        const Vec2 toAim = aim - hole.pos;
        const float dist = length(toAim);
        if (dist > 4.0F) {
            const float speedScale = std::clamp(dist / 220.0F, 0.25F, 1.0F);
            desiredVel = normalize(toAim) * (baseSpeed * speedScale);
        }
    }

    if (active) {
        const float t = 1.0F - std::exp(-cfg::kSteerResponse * cfg::kFixedDt);
        hole.vel = hole.vel + ((desiredVel - hole.vel) * t);
    } else {
        const float damping = std::exp(-cfg::kFriction * cfg::kFixedDt);
        hole.vel = hole.vel * damping;
        if (lengthSq(hole.vel) < 1.0F) {
            hole.vel = {0.0F, 0.0F};
        }
    }

    const float maxSpeed = baseSpeed * 1.08F;
    const float speed = length(hole.vel);
    if (speed > maxSpeed) {
        hole.vel = normalize(hole.vel) * maxSpeed;
    }
    hole.pos = hole.pos + (hole.vel * cfg::kFixedDt);
    hole.graceTimer = std::max(0.0F, hole.graceTimer - cfg::kFixedDt);
    clampHoleToWorld(hole);
}

void HoleWorld::resolveObjectInteractions() {
    for (HolePlayer& hole : holes_) {
        if (!hole.alive) {
            continue;
        }
        for (CityObject& object : objects_) {
            if (object.consumed) {
                continue;
            }
            const Vec2 delta = hole.pos - object.pos;
            const float d2 = lengthSq(delta);
            if (hole.radius + 1.0e-3F >= object.requiredRadius) {
                const float reach = (hole.radius * cfg::kConsumptionReach) + object.consumeRadius;
                if (d2 <= sqr(reach)) {
                    consume(object, hole);
                }
                continue;
            }

            const float blockRadius = std::max(8.0F, hole.radius * cfg::kHoleBlockFactor) +
                                      object.solidRadius + cfg::kObjectPushPadding;
            if (d2 >= sqr(blockRadius)) {
                continue;
            }

            const float d = std::sqrt(std::max(d2, 0.0F));
            const Vec2 normal = d > 1.0e-4F ? delta / d : Vec2{1.0F, 0.0F};
            hole.pos = object.pos + (normal * blockRadius);
            const float into = dot(hole.vel, normal);
            if (into < 0.0F) {
                hole.vel = hole.vel - (normal * into);
            }
            clampHoleToWorld(hole);
        }
    }
}

void HoleWorld::resolveRivalEating() {
    const std::size_t n = holes_.size();
    std::vector<char> eaten(n, 0);
    std::vector<std::size_t> eater(n, 0);

    for (std::size_t a = 0; a < n; ++a) {
        const HolePlayer& attacker = holes_.at(a);
        if (!attacker.alive || attacker.graceTimer > 0.0F) {
            continue;
        }
        for (std::size_t b = 0; b < n; ++b) {
            const HolePlayer& victim = holes_.at(b);
            if (a == b || !victim.alive || victim.graceTimer > 0.0F || eaten.at(b) != 0) {
                continue;
            }
            if (attacker.radius < victim.radius * cfg::kRivalEatSizeAdvantage) {
                continue;
            }
            const float reach = attacker.radius * cfg::kRivalEatRadiusFactor;
            if (distSq(attacker.pos, victim.pos) <= sqr(reach)) {
                eaten.at(b) = 1;
                eater.at(b) = a;
            }
        }
    }

    for (std::size_t victimIndex = 0; victimIndex < n; ++victimIndex) {
        if (eaten.at(victimIndex) == 0 || !holes_.at(victimIndex).alive) {
            continue;
        }
        const std::size_t attackerIndex = eater.at(victimIndex);
        HolePlayer& attacker = holes_.at(attackerIndex);
        HolePlayer& victim = holes_.at(victimIndex);
        if (attacker.alive) {
            attacker.score += cfg::kRivalScoreBonus + (victim.score * cfg::kRivalScoreScale);
            refreshHoleRadius(attackerIndex);
        }
        victim.alive = false;
        victim.vel = {0.0F, 0.0F};
        victim.graceTimer = 0.0F;
        victim.respawnTimer = victim.isBot ? cfg::kBotRespawnSeconds : 0.0F;
    }
}

void HoleWorld::updateRespawns() {
    for (std::size_t i = 1; i < holes_.size(); ++i) {
        HolePlayer& bot = holes_.at(i);
        if (bot.alive) {
            continue;
        }
        bot.respawnTimer = std::max(0.0F, bot.respawnTimer - cfg::kFixedDt);
        if (bot.respawnTimer <= 0.0F) {
            spawnBot(i);
        }
    }
}

void HoleWorld::consume(CityObject& object, HolePlayer& hole) {
    if (object.consumed) {
        return;
    }
    object.consumed = true;
    consumedMass_ += object.mass;
    ++consumedObjects_;
    hole.score += object.mass;
    hole.radius = radiusForScore(hole.score, difficultyIndex_);
}

void HoleWorld::clampHoleToWorld(HolePlayer& hole) const {
    const float margin = std::max(18.0F, hole.radius * 0.35F);
    const float minX = margin;
    const float maxX = worldW() - margin;
    const float minY = margin;
    const float maxY = worldH() - margin;
    hole.pos.x = std::clamp(hole.pos.x, minX, maxX);
    hole.pos.y = std::clamp(hole.pos.y, minY, maxY);
    if ((hole.pos.x <= minX && hole.vel.x < 0.0F) || (hole.pos.x >= maxX && hole.vel.x > 0.0F)) {
        hole.vel.x = 0.0F;
    }
    if ((hole.pos.y <= minY && hole.vel.y < 0.0F) || (hole.pos.y >= maxY && hole.vel.y > 0.0F)) {
        hole.vel.y = 0.0F;
    }
}

Vec2 HoleWorld::randomSpawnPosition(float clearance) {
    const float margin = std::max(clearance, 220.0F);
    Vec2 best = spawnCenter(profile_);
    const float minHoleGap = clearance + 260.0F;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const Vec2 pos{frand(margin, worldW() - margin), frand(margin, worldH() - margin)};
        best = pos;
        bool clear = true;
        for (const HolePlayer& hole : holes_) {
            if (!hole.alive) {
                continue;
            }
            const float gap = hole.radius + minHoleGap;
            if (distSq(pos, hole.pos) <= sqr(gap)) {
                clear = false;
                break;
            }
        }
        if (!clear) {
            continue;
        }
        for (const CityObject& object : objects_) {
            if (object.consumed) {
                continue;
            }
            const float gap = object.solidRadius + clearance;
            if (distSq(pos, object.pos) <= sqr(gap)) {
                clear = false;
                break;
            }
        }
        if (clear) {
            return pos;
        }
    }
    return best;
}

const CityObject* HoleWorld::nearestEdibleObject(Vec2 from, float radius, float maxDistance) const {
    const CityObject* best = nullptr;
    float bestD2 = sqr(maxDistance);
    for (const CityObject& object : objects_) {
        if (object.consumed || radius + 1.0e-3F < object.requiredRadius) {
            continue;
        }
        const float d2 = distSq(from, object.pos);
        if (d2 < bestD2) {
            bestD2 = d2;
            best = &object;
        }
    }
    return best;
}

const HolePlayer* HoleWorld::nearestThreat(const HolePlayer& self, float maxDistance) const {
    const HolePlayer* best = nullptr;
    float bestD2 = sqr(maxDistance);
    for (const HolePlayer& other : holes_) {
        if (&other == &self || !other.alive || other.graceTimer > 0.0F) {
            continue;
        }
        if (other.radius < self.radius * cfg::kRivalEatSizeAdvantage) {
            continue;
        }
        const float d2 = distSq(self.pos, other.pos);
        if (d2 < bestD2) {
            bestD2 = d2;
            best = &other;
        }
    }
    return best;
}

const HolePlayer* HoleWorld::nearestPrey(const HolePlayer& self, float maxDistance) const {
    const HolePlayer* best = nullptr;
    float bestD2 = sqr(maxDistance);
    for (const HolePlayer& other : holes_) {
        if (&other == &self || !other.alive || other.graceTimer > 0.0F) {
            continue;
        }
        if (self.radius < other.radius * cfg::kRivalEatSizeAdvantage) {
            continue;
        }
        const float d2 = distSq(self.pos, other.pos);
        if (d2 < bestD2) {
            bestD2 = d2;
            best = &other;
        }
    }
    return best;
}

Vec2 HoleWorld::inwardSteer(Vec2 pos) const {
    const float margin = std::min(760.0F, worldW() * 0.14F);
    Vec2 steer{0.0F, 0.0F};
    if (pos.x < margin) {
        steer.x += (margin - pos.x) / margin;
    }
    if (pos.x > worldW() - margin) {
        steer.x -= (pos.x - (worldW() - margin)) / margin;
    }
    if (pos.y < margin) {
        steer.y += (margin - pos.y) / margin;
    }
    if (pos.y > worldH() - margin) {
        steer.y -= (pos.y - (worldH() - margin)) / margin;
    }
    return steer;
}

} // namespace og::hole
