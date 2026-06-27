#include "games/nibbles/NibblesScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/FixedTimestep.hpp"
#include "core/GridLayout.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/nibbles/NibblesLevels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace og {
namespace {

constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 92.0F;
constexpr float kTopButtonRadius = 52.0F;

constexpr float kAreaX = 32.0F;
constexpr float kAreaTop = 282.0F;
constexpr float kAreaBottom = 820.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = kAreaBottom - kAreaTop;
constexpr float kMaxCellPx = 8.0F;

constexpr float kDpadCx = layout::kWidthF / 2.0F;
constexpr float kDpadCy = 1190.0F;
constexpr float kDpadGap = 120.0F;
constexpr float kDpadRadius = 55.0F;
constexpr float kButtonRowY = 850.0F;
constexpr float kSwipeThreshold = 44.0F;

constexpr Color kField = rgb(30, 34, 42);
constexpr Color kFieldEdge = rgb(18, 20, 26);
constexpr Color kWall = rgb(88, 92, 108);
constexpr Color kWallTop = rgb(132, 138, 158);
constexpr Color kWarp = rgb(112, 86, 190);
constexpr Color kWarpCore = rgb(178, 144, 238);
constexpr Color kRegular = rgb(246, 198, 70);
constexpr Color kHalf = rgb(74, 188, 218);
constexpr Color kDouble = rgb(86, 194, 124);
constexpr Color kLife = rgb(232, 88, 106);
constexpr Color kReverse = rgb(206, 116, 226);
constexpr Color kFake = rgb(160, 160, 168);
constexpr Color kHeadEye = rgb(20, 22, 28);

constexpr std::array<Color, 6> kWormColors{
    rgb(226, 70, 70),  rgb(76, 187, 122), rgb(74, 144, 236),
    rgb(236, 206, 72), rgb(50, 198, 210), rgb(190, 98, 210),
};

[[nodiscard]] int difficultyIndex(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return 0;
    case Difficulty::Hard:
        return 2;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break;
    }
    return 1;
}

[[nodiscard]] int& savedLevelField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.nibblesLevelEasy;
    case Difficulty::Medium:
        return s.nibblesLevelMedium;
    case Difficulty::Hard:
        return s.nibblesLevelHard;
    case Difficulty::VeryHard:
        break;
    }
    return s.nibblesLevelHard;
}

void setSavedLevel(Difficulty difficulty, int level) {
    savedLevelField(settings(), difficulty) = std::max(1, level);
    saveSettings(settings());
}

[[nodiscard]] nibbles::NibblesLevel fallbackLevel() {
    nibbles::NibblesLevel level;
    level.width = 20;
    level.height = 16;
    level.sourceLevel = 1;
    const auto cellIndex = [&level](int x, int y) {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(level.width)) +
               static_cast<std::size_t>(x);
    };
    const std::size_t cellCount =
        static_cast<std::size_t>(level.width) * static_cast<std::size_t>(level.height);
    level.cells.assign(cellCount, nibbles::Cell::Empty);
    for (int x = 0; x < level.width; ++x) {
        level.cells.at(cellIndex(x, 0)) = nibbles::Cell::Wall;
        level.cells.at(cellIndex(x, level.height - 1)) = nibbles::Cell::Wall;
    }
    for (int y = 0; y < level.height; ++y) {
        level.cells.at(cellIndex(0, y)) = nibbles::Cell::Wall;
        level.cells.at(cellIndex(level.width - 1, y)) = nibbles::Cell::Wall;
    }
    level.spawns = {
        nibbles::Spawn{.pos = {.x = 4, .y = 4}, .direction = nibbles::Direction::Right},
        nibbles::Spawn{.pos = {.x = 15, .y = 4}, .direction = nibbles::Direction::Left},
        nibbles::Spawn{.pos = {.x = 4, .y = 12}, .direction = nibbles::Direction::Right},
        nibbles::Spawn{.pos = {.x = 15, .y = 12}, .direction = nibbles::Direction::Left},
    };
    return level;
}

[[nodiscard]] nibbles::NibblesLevel levelFor(int level) {
    if (const nibbles::NibblesLevel* loaded = nibbles::nibblesLevel(level); loaded != nullptr) {
        return *loaded;
    }
    return fallbackLevel();
}

[[nodiscard]] Color withAlpha(Color color, std::uint8_t alpha) {
    color.a = alpha;
    return color;
}

[[nodiscard]] Color bonusColor(const nibbles::Bonus& bonus) {
    if (bonus.fake) {
        return kFake;
    }
    switch (bonus.type) {
    case nibbles::BonusType::Regular:
        return kRegular;
    case nibbles::BonusType::Half:
        return kHalf;
    case nibbles::BonusType::Double:
        return kDouble;
    case nibbles::BonusType::Life:
        return kLife;
    case nibbles::BonusType::Reverse:
        return kReverse;
    }
    return kRegular;
}

[[nodiscard]] const char* bonusLabel(const nibbles::Bonus& bonus) {
    if (bonus.fake) {
        return "?";
    }
    switch (bonus.type) {
    case nibbles::BonusType::Regular:
        return "*";
    case nibbles::BonusType::Half:
        return "1/2";
    case nibbles::BonusType::Double:
        return "2x";
    case nibbles::BonusType::Life:
        return "+";
    case nibbles::BonusType::Reverse:
        return "R";
    }
    return "*";
}

[[nodiscard]] nibbles::Direction directionFromSwipe(float dx, float dy) {
    if (std::abs(dx) > std::abs(dy)) {
        return dx < 0.0F ? nibbles::Direction::Left : nibbles::Direction::Right;
    }
    return dy < 0.0F ? nibbles::Direction::Up : nibbles::Direction::Down;
}

} // namespace

int nibblesSavedLevel(Difficulty difficulty) {
    return savedLevelField(settings(), difficulty);
}

NibblesScene::NibblesScene(SceneManager& manager, Difficulty difficulty, int level)
    : manager_(manager), difficulty_(difficulty), level_(std::max(1, level)),
      world_(levelFor(level_), nibbles::nibblesConfigForDifficulty(difficultyIndex(difficulty_)),
             std::random_device{}()),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kTopButtonRadius),
      upButton_(IconButton::Icon::Glyph, kDpadCx, kDpadCy - kDpadGap, kDpadRadius),
      downButton_(IconButton::Icon::Glyph, kDpadCx, kDpadCy + kDpadGap, kDpadRadius),
      leftButton_(IconButton::Icon::Glyph, kDpadCx - kDpadGap, kDpadCy, kDpadRadius),
      rightButton_(IconButton::Icon::Glyph, kDpadCx + kDpadGap, kDpadCy, kDpadRadius),
      overlay_(color(difficulty_), colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    upButton_.setGlyph("^", 58.0F);
    downButton_.setGlyph("v", 58.0F);
    leftButton_.setGlyph("<", 58.0F);
    rightButton_.setGlyph(">", 58.0F);
    upButton_.setOnTap([this] { queueDirection(nibbles::Direction::Up); });
    downButton_.setOnTap([this] { queueDirection(nibbles::Direction::Down); });
    leftButton_.setOnTap([this] { queueDirection(nibbles::Direction::Left); });
    rightButton_.setOnTap([this] { queueDirection(nibbles::Direction::Right); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    layoutBoard();
    snapshotBodies();
}

void NibblesScene::layoutBoard() {
    const grid::Fit fit = grid::fitCentered(kAreaX, kAreaTop, kAreaW, kAreaH, world_.level().width,
                                            world_.level().height, kMaxCellPx);
    cellPx_ = fit.cellPx;
    originX_ = fit.originX;
    originY_ = fit.originY;
    rebuildBoardMesh();
}

void NibblesScene::rebuildBoardMesh() {
    boardVerts_.clear();
    boardIndices_.clear();

    const float gap = std::max(0.4F, cellPx_ * 0.08F);
    const float tile = std::max(1.0F, cellPx_ - (2.0F * gap));
    const auto addQuad = [this](float x, float y, float w, float h, Color color) {
        const int base = static_cast<int>(boardVerts_.size());
        boardVerts_.push_back(Canvas::Vertex{.x = x, .y = y, .color = color});
        boardVerts_.push_back(Canvas::Vertex{.x = x + w, .y = y, .color = color});
        boardVerts_.push_back(Canvas::Vertex{.x = x + w, .y = y + h, .color = color});
        boardVerts_.push_back(Canvas::Vertex{.x = x, .y = y + h, .color = color});
        boardIndices_.push_back(base + 0);
        boardIndices_.push_back(base + 1);
        boardIndices_.push_back(base + 2);
        boardIndices_.push_back(base + 0);
        boardIndices_.push_back(base + 2);
        boardIndices_.push_back(base + 3);
    };

    for (int y = 0; y < world_.level().height; ++y) {
        for (int x = 0; x < world_.level().width; ++x) {
            const nibbles::Cell cell = world_.cellAt(x, y);
            if (cell == nibbles::Cell::Empty) {
                continue;
            }

            const float px = cellX(x) + gap;
            const float py = cellY(y) + gap;
            if (cell == nibbles::Cell::Wall) {
                addQuad(px, py, tile, tile, kWall);
                addQuad(px, py, tile, std::max(1.0F, tile * 0.22F), kWallTop);
            } else {
                addQuad(px, py, tile, tile, kWarp);
                const float core = std::max(1.0F, tile * 0.5F);
                addQuad(px + ((tile - core) * 0.5F), py + ((tile - core) * 0.5F), core, core,
                        kWarpCore);
            }
        }
    }
}

void NibblesScene::snapshotBodies() {
    previousBodies_.clear();
    previousBodies_.reserve(world_.worms().size());
    for (const nibbles::Worm& worm : world_.worms()) {
        previousBodies_.push_back(worm.body);
    }
}

void NibblesScene::queueDirection(nibbles::Direction direction) {
    if (phase_ == Phase::Playing) {
        world_.queueTurn(direction);
    }
}

void NibblesScene::handleSwipe(const PointerEvent& event) {
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        swiping_ = true;
        swipeStartX_ = event.x;
        swipeStartY_ = event.y;
        break;
    case PointerEvent::Phase::Move:
    case PointerEvent::Phase::Up: {
        if (!swiping_) {
            return;
        }
        const float dx = event.x - swipeStartX_;
        const float dy = event.y - swipeStartY_;
        if ((dx * dx) + (dy * dy) >= kSwipeThreshold * kSwipeThreshold) {
            queueDirection(directionFromSwipe(dx, dy));
            swipeStartX_ = event.x;
            swipeStartY_ = event.y;
        }
        if (event.phase == PointerEvent::Phase::Up) {
            swiping_ = false;
        }
        break;
    }
    }
}

void NibblesScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ != Phase::Playing) {
        overlay_.handleInput(event);
        return;
    }
    if (upButton_.handleInput(event) || downButton_.handleInput(event) ||
        leftButton_.handleInput(event) || rightButton_.handleInput(event)) {
        return;
    }
    handleSwipe(event);
}

void NibblesScene::update(float dtSeconds) {
    if (phase_ == Phase::Playing) {
        advanceFixed(accum_, dtSeconds, world_.tickSeconds(), 0.5F, [this] {
            snapshotBodies();
            world_.step();
        });
        if (world_.status() == nibbles::NibblesStatus::LevelComplete) {
            enterLevelClear();
        } else if (world_.status() == nibbles::NibblesStatus::GameOver) {
            enterGameOver();
        }
    }
}

void NibblesScene::enterLevelClear() {
    if (resolved_) {
        return;
    }
    resolved_ = true;
    phase_ = Phase::LevelClear;
    const int lastLevel = std::max(1, nibbles::nibblesLevelCount());
    const int nextLevel = level_ >= lastLevel ? 1 : level_ + 1;
    setSavedLevel(difficulty_, nextLevel);
    overlay_.setActionLabel("NEXT");
    overlay_.setOnAction([this, nextLevel] {
        manager_.replace(std::make_unique<NibblesScene>(manager_, difficulty_, nextLevel));
    });
}

void NibblesScene::enterGameOver() {
    if (resolved_) {
        return;
    }
    resolved_ = true;
    phase_ = Phase::GameOver;
    overlay_.setActionLabel("RETRY");
    overlay_.setOnAction([this] {
        manager_.replace(std::make_unique<NibblesScene>(manager_, difficulty_, level_));
    });
}

float NibblesScene::cellX(int x) const {
    return originX_ + (static_cast<float>(x) * cellPx_);
}

float NibblesScene::cellY(int y) const {
    return originY_ + (static_cast<float>(y) * cellPx_);
}

float NibblesScene::cellCenterX(int x) const {
    return cellX(x) + (cellPx_ / 2.0F);
}

float NibblesScene::cellCenterY(int y) const {
    return cellY(y) + (cellPx_ / 2.0F);
}

float NibblesScene::renderAlpha() const {
    return std::clamp(accum_ / std::max(world_.tickSeconds(), 0.001F), 0.0F, 1.0F);
}

void NibblesScene::drawTopBar(Canvas& canvas) const {
    backButton_.render(canvas);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, 48.0F, 28.0F,
                        color(difficulty_));
    canvas.textCentered("Nibbles", layout::kWidthF / 2.0F, 104.0F, 52.0F, theme().titleText);
    canvas.textCentered("Level " + std::to_string(level_), layout::kWidthF / 2.0F, 156.0F, 28.0F,
                        theme().mutedText);
    const std::string stats = "Score " + std::to_string(world_.score()) + "   Lives " +
                              std::to_string(world_.lives()) + "   Left " +
                              std::to_string(world_.regularLeft());
    canvas.textCentered(stats, layout::kWidthF / 2.0F, 214.0F, 28.0F, theme().bodyText);
}

void NibblesScene::drawBoard(Canvas& canvas) const {
    const float boardW = static_cast<float>(world_.level().width) * cellPx_;
    const float boardH = static_cast<float>(world_.level().height) * cellPx_;
    canvas.fillRect(originX_ - 8.0F, originY_ - 8.0F, boardW + 16.0F, boardH + 16.0F, kFieldEdge);
    canvas.fillRect(originX_, originY_, boardW, boardH, kField);
    canvas.fillMesh(boardVerts_, boardIndices_);
}

void NibblesScene::drawBonuses(Canvas& canvas) const {
    for (const nibbles::Bonus& bonus : world_.bonuses()) {
        const float px = cellX(bonus.pos.x);
        const float py = cellY(bonus.pos.y);
        const float size = cellPx_ * 2.0F;
        const Color fill = bonusColor(bonus);
        canvas.fillRoundedRect(px + 1.0F, py + 1.0F, std::max(2.0F, size - 2.0F),
                               std::max(2.0F, size - 2.0F), std::max(2.0F, cellPx_ * 0.35F), fill);
        if (cellPx_ >= 6.0F) {
            canvas.textCentered(bonusLabel(bonus), px + (size * 0.5F), py + (size * 0.50F),
                                std::max(9.0F, cellPx_ * 1.15F), colors::white);
        }
    }
}

void NibblesScene::drawWorms(Canvas& canvas) const {
    const float gap = std::max(0.35F, cellPx_ * 0.08F);
    const float tile = std::max(1.0F, cellPx_ - (2.0F * gap));
    const float alpha = renderAlpha();
    for (const nibbles::Worm& worm : world_.worms()) {
        if (!worm.alive()) {
            continue;
        }
        Color color = kWormColors.at(static_cast<std::size_t>(worm.id) % kWormColors.size());
        if (!worm.materialized()) {
            color = withAlpha(color, 145);
        }
        const auto* previous = static_cast<std::size_t>(worm.id) < previousBodies_.size()
                                   ? &previousBodies_.at(static_cast<std::size_t>(worm.id))
                                   : nullptr;
        for (std::size_t i = worm.body.size(); i > 0; --i) {
            const nibbles::Position pos = worm.body.at(i - 1);
            nibbles::Position prev = pos;
            if (previous != nullptr && i - 1 < previous->size()) {
                prev = previous->at(i - 1);
            }
            auto drawX = static_cast<float>(pos.x);
            auto drawY = static_cast<float>(pos.y);
            const int dx = pos.x - prev.x;
            const int dy = pos.y - prev.y;
            if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
                drawX = static_cast<float>(prev.x) + (static_cast<float>(dx) * alpha);
                drawY = static_cast<float>(prev.y) + (static_cast<float>(dy) * alpha);
            }
            const float px = originX_ + (drawX * cellPx_) + gap;
            const float py = originY_ + (drawY * cellPx_) + gap;
            const bool head = i == 1;
            canvas.fillRect(px, py, tile, tile, color);
            if (head && cellPx_ >= 6.0F) {
                const float eye = std::max(1.0F, tile * 0.28F);
                canvas.fillRect(px + ((tile - eye) * 0.5F), py + ((tile - eye) * 0.5F), eye, eye,
                                kHeadEye);
            }
        }
    }
}

void NibblesScene::drawDpad(Canvas& canvas) const {
    upButton_.render(canvas);
    downButton_.render(canvas);
    leftButton_.render(canvas);
    rightButton_.render(canvas);
}

void NibblesScene::drawOverlay(Canvas& canvas) const {
    const bool won = phase_ == Phase::LevelClear;
    overlay_.render(canvas, won ? "LEVEL CLEAR" : "GAME OVER", 610.0F, won ? 78.0F : 88.0F);
    canvas.textCentered("Score " + std::to_string(world_.score()), layout::kWidthF / 2.0F, 735.0F,
                        38.0F, colors::white);
}

void NibblesScene::render(Canvas& canvas) {
    canvas.clear(theme().menuBg);
    drawTopBar(canvas);
    drawBoard(canvas);
    drawBonuses(canvas);
    drawWorms(canvas);
    drawDpad(canvas);
    if (phase_ != Phase::Playing) {
        drawOverlay(canvas);
    }
}

} // namespace og
