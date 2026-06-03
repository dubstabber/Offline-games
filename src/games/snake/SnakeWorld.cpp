#include "games/snake/SnakeWorld.hpp"

#include <algorithm>
#include <cmath>

namespace og::snake {
namespace {

using namespace config;

constexpr std::size_t kCollisionStride = 2; // test every Nth body sample
constexpr float kPlayerStartScore = 10.0F;

[[nodiscard]] float foodRadiusFor(float mass) {
    return std::min(kFoodBaseRadius + (mass * kFoodRadiusPerMass), kFoodRadiusCap);
}

} // namespace

SnakeWorld::SnakeWorld(int difficultyIndex, std::uint32_t seed)
    : difficultyIndex_(std::clamp(difficultyIndex, 0, 2)), rng_(seed) {
    snakes_.resize(static_cast<std::size_t>(kBotCount) + 1);

    // Player at index 0.
    Snake& p = snakes_.front();
    p.isBot = false;
    p.name = "You";
    p.gradIndex = 0;
    p.score = kPlayerStartScore;
    p.invulnTimer = kInvulnSeconds;
    p.alive = true;
    initGeometry(p);
    const float ph = frand(0.0F, 2.0F * kPi);
    buildBody(p, {worldSize_ * 0.5F, worldSize_ * 0.5F}, ph);

    for (std::size_t i = 1; i < snakes_.size(); ++i) {
        spawnBot(snakes_.at(i));
    }

    food_.reserve(static_cast<std::size_t>(kFoodTargetCount) + 64);
    for (int i = 0; i < kFoodTargetCount; ++i) {
        spawnFoodOrb();
    }
}

int SnakeWorld::segmentCountForScore(float score) {
    const float s = std::max(0.0F, score);
    const float n = static_cast<float>(kBaseSegments) + (kSegmentsPerSqrtScore * std::sqrt(s));
    return std::clamp(static_cast<int>(n), kBaseSegments, kMaxSegments);
}

float SnakeWorld::radiusForScore(float score) {
    const float s = std::max(0.0F, score);
    const float r = kBaseRadius * (1.0F + (kRadiusGrowth * std::log1p(s * kRadiusScoreScale)));
    return std::min(r, kRadiusCap);
}

int SnakeWorld::liveSnakeCount() const {
    int count = 0;
    for (const Snake& s : snakes_) {
        count += s.alive ? 1 : 0;
    }
    return count;
}

void SnakeWorld::initGeometry(Snake& snake) {
    snake.radius = radiusForScore(snake.score);
    snake.spacing = snake.radius * kSpacingFactor;
}

void SnakeWorld::buildBody(Snake& snake, Vec2 at, float heading) {
    snake.head = at;
    snake.heading = heading;
    snake.targetHeading = heading;
    snake.pathAccum = 0.0F;
    snake.path.clear();
    const int segments = segmentCountForScore(snake.score);
    const Vec2 back = fromAngle(heading + kPi);
    for (int i = 0; i < segments; ++i) {
        snake.path.push_back(at + (back * (snake.spacing * static_cast<float>(i))));
    }
}

void SnakeWorld::spawnBot(Snake& snake) {
    snake.isBot = true;
    snake.alive = true;
    snake.boosting = false;
    snake.boostDropAccum = 0.0F;
    snake.invulnTimer = kInvulnSeconds;
    snake.name = std::string(pickName());
    snake.gradIndex = static_cast<std::uint8_t>(irand(1, kSnakeGradientCount - 1));
    snake.score = frand(8.0F, 140.0F);

    BotConfig botCfg = botPresetFor(difficultyIndex_);
    botCfg.crashAvoidance = std::clamp(botCfg.crashAvoidance + frand(-0.1F, 0.1F), 0.2F, 1.2F);
    botCfg.aggressiveness = std::max(0.0F, botCfg.aggressiveness + frand(-0.1F, 0.15F));
    botCfg.maxTurn += frand(-0.4F, 0.4F);
    snake.bot = botCfg;

    initGeometry(snake);
    const Vec2 pos = randomSafePosition(snake.radius * 12.0F);
    buildBody(snake, pos, frand(0.0F, 2.0F * kPi));
}

std::string_view SnakeWorld::pickName() {
    return config::kNamePool.at(
        static_cast<std::size_t>(irand(0, static_cast<int>(kNamePool.size()) - 1)));
}

void SnakeWorld::spawnFoodOrb() {
    const float margin = 40.0F;
    const Vec2 pos{frand(margin, worldSize_ - margin), frand(margin, worldSize_ - margin)};
    food_.push_back(makeWeightedFood(pos));
}

FoodOrb SnakeWorld::makeWeightedFood(Vec2 pos) {
    float total = 0.0F;
    for (const FoodPack& pack : kFoodPacks) {
        total += pack.weight;
    }
    float roll = frand(0.0F, total);
    float mass = kFoodPacks.front().mass;
    for (const FoodPack& pack : kFoodPacks) {
        if (roll < pack.weight) {
            mass = pack.mass;
            break;
        }
        roll -= pack.weight;
    }
    FoodOrb orb;
    orb.pos = pos;
    orb.mass = mass;
    orb.radius = foodRadiusFor(mass);
    orb.colorIndex = static_cast<std::uint8_t>(irand(0, kFoodColorCount - 1));
    return orb;
}

void SnakeWorld::step() {
    stepBotBrains();
    integrateMovement();
    advanceTrails();
    resolveFoodPickup();
    resolveCollisions();
    respawnDead();
    maintainFood();
}

const FoodOrb* SnakeWorld::nearestFood(Vec2 from, float radius) const {
    const FoodOrb* best = nullptr;
    float bestD = radius * radius;
    for (const FoodOrb& f : food_) {
        const float d = distSq(from, f.pos);
        if (d < bestD) {
            bestD = d;
            best = &f;
        }
    }
    return best;
}

const Snake* SnakeWorld::findPrey(std::size_t index, float maxScoreRatio) const {
    const Snake& b = snakes_.at(index);
    const Snake* prey = nullptr;
    float preyD = 800.0F * 800.0F;
    for (std::size_t j = 0; j < snakes_.size(); ++j) {
        const Snake& other = snakes_.at(j);
        if (j == index || !other.alive || other.score >= b.score * maxScoreRatio) {
            continue;
        }
        const float d = distSq(b.head, other.head);
        if (d < preyD) {
            preyD = d;
            prey = &other;
        }
    }
    return prey;
}

void SnakeWorld::steerBot(std::size_t index) {
    Snake& b = snakes_.at(index);
    Vec2 dir = fromAngle(b.heading); // momentum baseline

    if (const FoodOrb* food = nearestFood(b.head, b.bot.foodSeekRadius); food != nullptr) {
        dir = dir + (normalize(food->pos - b.head) * 1.4F);
    }

    bool attacking = false;
    if (b.bot.aggressiveness > 0.0F) {
        if (const Snake* prey = findPrey(index, 0.85F); prey != nullptr) {
            const Vec2 lead = prey->head + (fromAngle(prey->heading) * 160.0F);
            dir = dir + (normalize(lead - b.head) * (1.5F * b.bot.aggressiveness));
            attacking = true;
        }
    }

    dir = dir + (inwardSteer(b.head) * (1.5F + (b.bot.crashAvoidance * 2.5F)));

    if (const int turn = bodyAvoidTurn(index); turn != 0) {
        const Vec2 dodge = fromAngle(b.heading + (static_cast<float>(turn) * 0.9F));
        dir = (dodge * (1.0F + (b.bot.crashAvoidance * 3.0F))) + (dir * 0.2F);
    }

    if (lengthSq(dir) > 1e-4F) {
        b.targetHeading = wrapAngle(angleOf(dir) + frand(-0.04F, 0.04F));
    }
    b.boosting = attacking && b.score > kBoostMinScore && b.bot.aggressiveness > 0.5F;
}

void SnakeWorld::stepBotBrains() {
    Snake& human = snakes_.front();
    if (human.alive) {
        const Vec2 to = input_.aimWorld - human.head;
        if (lengthSq(to) > 4.0F) {
            human.targetHeading = angleOf(to);
        }
        human.boosting = input_.boost && human.score > kBoostMinScore;
    }
    for (std::size_t i = 1; i < snakes_.size(); ++i) {
        if (snakes_.at(i).alive) {
            steerBot(i);
        }
    }
}

void SnakeWorld::integrateMovement() {
    for (Snake& s : snakes_) {
        if (!s.alive) {
            continue;
        }
        const float maxTurn = (s.isBot ? s.bot.maxTurn : kMaxTurnRate) * kFixedDt;
        s.heading = rotateToward(s.heading, s.targetHeading, maxTurn);
        s.speed = kBaseSpeed * (s.boosting ? kBoostMultiplier : 1.0F);
        s.head = s.head + (fromAngle(s.heading) * (s.speed * kFixedDt));

        if (s.boosting && s.score > kBoostMinScore) {
            const float bleed = kBoostMassBleedPerSec * kFixedDt;
            s.score = std::max(kBoostMinScore, s.score - bleed);
            s.boostDropAccum += bleed;
            if (s.boostDropAccum >= kBoostDropMass) {
                s.boostDropAccum -= kBoostDropMass;
                const Vec2 tail = s.path.empty() ? s.head : s.path.back();
                FoodOrb orb;
                orb.pos = tail;
                orb.mass = kBoostDropMass;
                orb.radius = foodRadiusFor(kBoostDropMass);
                orb.colorIndex = static_cast<std::uint8_t>(irand(0, kFoodColorCount - 1));
                food_.push_back(orb);
            }
        }

        initGeometry(s); // radius/spacing follow the (possibly changed) score
        s.invulnTimer = std::max(0.0F, s.invulnTimer - kFixedDt);
    }
}

void SnakeWorld::advanceTrails() {
    for (Snake& s : snakes_) {
        if (!s.alive) {
            continue;
        }
        s.pathAccum += s.speed * kFixedDt;
        while (s.spacing > 0.0F && s.pathAccum >= s.spacing) {
            s.path.push_front(s.head);
            s.pathAccum -= s.spacing;
        }
        if (s.path.empty()) {
            s.path.push_front(s.head);
        }
        const auto segments = static_cast<std::size_t>(segmentCountForScore(s.score));
        while (s.path.size() > segments) {
            s.path.pop_back();
        }
    }
}

void SnakeWorld::resolveFoodPickup() {
    for (Snake& s : snakes_) {
        if (!s.alive) {
            continue;
        }
        std::size_t k = 0;
        while (k < food_.size()) {
            const FoodOrb& orb = food_.at(k);
            const float reach = s.radius + orb.radius;
            if (distSq(s.head, orb.pos) <= reach * reach) {
                s.score += orb.mass;
                food_.at(k) = food_.back();
                food_.pop_back();
            } else {
                ++k;
            }
        }
    }
}

void SnakeWorld::resolveCollisions() {
    std::vector<char> dead(snakes_.size(), 0);
    for (std::size_t a = 0; a < snakes_.size(); ++a) {
        const Snake& self = snakes_.at(a);
        if (!self.alive || self.invulnTimer > 0.0F) {
            continue;
        }
        // World edge.
        if (self.head.x < self.radius || self.head.x > worldSize_ - self.radius ||
            self.head.y < self.radius || self.head.y > worldSize_ - self.radius) {
            dead.at(a) = 1;
            continue;
        }
        // Head into another snake's body.
        for (std::size_t b = 0; b < snakes_.size(); ++b) {
            if (b == a || !snakes_.at(b).alive) {
                continue;
            }
            if (headHitsBody(self, snakes_.at(b))) {
                dead.at(a) = 1;
                break;
            }
        }
    }
    for (std::size_t a = 0; a < snakes_.size(); ++a) {
        if (dead.at(a) != 0 && snakes_.at(a).alive) {
            snakes_.at(a).alive = false;
            dropDeathLoot(snakes_.at(a));
        }
    }
}

void SnakeWorld::respawnDead() {
    // Bots respawn immediately to keep the population; the player stays dead.
    for (std::size_t i = 1; i < snakes_.size(); ++i) {
        if (!snakes_.at(i).alive) {
            spawnBot(snakes_.at(i));
        }
    }
}

void SnakeWorld::maintainFood() {
    int need = kFoodTargetCount - static_cast<int>(food_.size());
    need = std::min(need, kFoodMaxSpawnPerStep);
    for (int i = 0; i < need; ++i) {
        spawnFoodOrb();
    }
}

float SnakeWorld::bodyReach(const Snake& snake) {
    return (static_cast<float>(snake.path.size()) * snake.spacing) + snake.radius;
}

bool SnakeWorld::headHitsBody(const Snake& head, const Snake& body) {
    const float reach = bodyReach(body) + head.radius;
    if (distSq(head.head, body.head) > reach * reach) {
        return false;
    }
    const float rr = head.radius + body.radius;
    const float rr2 = rr * rr;
    for (std::size_t k = 0; k < body.path.size(); k += kCollisionStride) {
        if (distSq(head.head, body.path.at(k)) <= rr2) {
            return true;
        }
    }
    return false;
}

bool SnakeWorld::pointHitsBody(Vec2 point, std::size_t excludeIndex, float margin) const {
    for (std::size_t i = 0; i < snakes_.size(); ++i) {
        const Snake& s = snakes_.at(i);
        if (i == excludeIndex || !s.alive) {
            continue;
        }
        const float reach = bodyReach(s) + margin;
        if (distSq(point, s.head) > reach * reach) {
            continue;
        }
        const float rr = s.radius + margin;
        const float rr2 = rr * rr;
        if (distSq(point, s.head) <= rr2) {
            return true;
        }
        for (std::size_t k = 0; k < s.path.size(); k += kCollisionStride) {
            if (distSq(point, s.path.at(k)) <= rr2) {
                return true;
            }
        }
    }
    return false;
}

int SnakeWorld::bodyAvoidTurn(std::size_t index) const {
    const Snake& b = snakes_.at(index);
    const float ahead = kBotLookAhead + b.radius;
    const auto blocked = [&](float angle) {
        const Vec2 probe = b.head + (fromAngle(angle) * ahead);
        const bool edge = probe.x < b.radius || probe.x > worldSize_ - b.radius ||
                          probe.y < b.radius || probe.y > worldSize_ - b.radius;
        return edge || pointHitsBody(probe, index, b.radius * 0.5F);
    };
    if (!blocked(b.heading)) {
        return 0;
    }
    const bool left = blocked(b.heading - 0.8F);
    const bool right = blocked(b.heading + 0.8F);
    if (left && !right) {
        return 1;
    }
    if (right && !left) {
        return -1;
    }
    // Both (or neither of the side probes) blocked: turn toward arena center.
    const Vec2 toCenter{(worldSize_ * 0.5F) - b.head.x, (worldSize_ * 0.5F) - b.head.y};
    const float rel = wrapAngle(angleOf(toCenter) - b.heading);
    return rel >= 0.0F ? 1 : -1;
}

Vec2 SnakeWorld::inwardSteer(Vec2 pos) const {
    Vec2 s{0.0F, 0.0F};
    if (pos.x < kBotEdgeMargin) {
        s.x += (kBotEdgeMargin - pos.x) / kBotEdgeMargin;
    }
    if (pos.x > worldSize_ - kBotEdgeMargin) {
        s.x -= (pos.x - (worldSize_ - kBotEdgeMargin)) / kBotEdgeMargin;
    }
    if (pos.y < kBotEdgeMargin) {
        s.y += (kBotEdgeMargin - pos.y) / kBotEdgeMargin;
    }
    if (pos.y > worldSize_ - kBotEdgeMargin) {
        s.y -= (pos.y - (worldSize_ - kBotEdgeMargin)) / kBotEdgeMargin;
    }
    return s;
}

Vec2 SnakeWorld::randomSafePosition(float clearance) {
    const float margin = std::max(clearance, kBotEdgeMargin * 0.5F);
    Vec2 best{worldSize_ * 0.5F, worldSize_ * 0.5F};
    for (int attempt = 0; attempt < 24; ++attempt) {
        const Vec2 pos{frand(margin, worldSize_ - margin), frand(margin, worldSize_ - margin)};
        best = pos;
        if (!pointHitsBody(pos, snakes_.size(), clearance)) {
            return pos;
        }
    }
    return best;
}

void SnakeWorld::dropDeathLoot(const Snake& dead) {
    const float lootScore = dead.score * kDeathLoot.at(static_cast<std::size_t>(difficultyIndex_));
    if (lootScore < kDeathOrbMinMass) {
        return;
    }
    const int orbCount = std::clamp(static_cast<int>(lootScore / 3.0F) + 1, 1, kMaxDeathOrbs);
    const float per = std::max(kDeathOrbMinMass, lootScore / static_cast<float>(orbCount));
    const std::size_t samples = dead.path.size();
    for (int i = 0; i < orbCount; ++i) {
        Vec2 pos = dead.head;
        if (samples > 0) {
            const float frac =
                orbCount > 1 ? static_cast<float>(i) / static_cast<float>(orbCount - 1) : 0.0F;
            const auto idx = static_cast<std::size_t>(frac * static_cast<float>(samples - 1));
            pos = dead.path.at(idx);
        }
        pos = pos + Vec2{frand(-12.0F, 12.0F), frand(-12.0F, 12.0F)};
        FoodOrb orb;
        orb.pos = pos;
        orb.mass = per;
        orb.radius = foodRadiusFor(per);
        orb.colorIndex = static_cast<std::uint8_t>(irand(0, kFoodColorCount - 1));
        food_.push_back(orb);
    }
}

float SnakeWorld::frand(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

int SnakeWorld::irand(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

} // namespace og::snake
