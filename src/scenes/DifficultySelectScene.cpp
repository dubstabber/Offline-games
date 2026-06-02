#include "scenes/DifficultySelectScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace og {
namespace {

// ---- Back button (circular, top-left) -----------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr Color kChevron = rgb(176, 124, 162);

// ---- Slider --------------------------------------------------------------
constexpr float kTrackX = 160.0F;
constexpr float kTrackW = 400.0F;
constexpr float kTrackCy = 820.0F;
constexpr float kTrackH = 28.0F;
constexpr float kKnobRadius = 34.0F;
constexpr Color kTrackBg = rgb(225, 210, 190);

// ---- PLAY button ---------------------------------------------------------
constexpr float kPlayW = 400.0F;
constexpr float kPlayH = 130.0F;
constexpr float kPlayX = (layout::kWidthF - kPlayW) / 2.0F;
constexpr float kPlayY = 1020.0F;

constexpr float kDescMaxWidth = 600.0F;
constexpr Color kDescColor = rgb(96, 84, 80);

const char* faceFor(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return "\xF0\x9F\x99\x82"; // 🙂
    case Difficulty::Medium:
        return "\xF0\x9F\x98\x90"; // 😐
    case Difficulty::Hard:
        return "\xF0\x9F\x98\x88"; // 😈
    }
    return "";
}

// x of the knob centre for a difficulty (Easy = left, Hard = right).
float stopX(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return kTrackX;
    case Difficulty::Medium:
        return kTrackX + (kTrackW / 2.0F);
    case Difficulty::Hard:
        return kTrackX + kTrackW;
    }
    return kTrackX;
}

// Greedy word-wrap to a max logical width using the canvas's text metrics.
std::vector<std::string> wrapText(Canvas& canvas, const std::string& text, float pixelSize,
                                  float maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t space = text.find(' ', pos);
        const std::string word = text.substr(pos, space - pos);
        std::string candidate = line;
        if (!candidate.empty()) {
            candidate += " ";
        }
        candidate += word;
        if (!line.empty() && canvas.measure(candidate, pixelSize).w > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        if (space == std::string::npos) {
            break;
        }
        pos = space + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

DifficultySelectScene::DifficultySelectScene(SceneManager& manager, GameInfo info)
    : manager_(manager), info_(std::move(info)), titleUpper_(info_.title),
      playButton_("PLAY", kPlayX, kPlayY, kPlayW, kPlayH) {
    std::ranges::transform(titleUpper_, titleUpper_.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    setDifficulty(difficulty_);
    playButton_.setOnTap([this] { manager_.push(info_.create(manager_, difficulty_)); });
}

void DifficultySelectScene::setDifficulty(Difficulty difficulty) {
    difficulty_ = difficulty;
    playButton_.setColors(color(difficulty_), colors::white);
}

bool DifficultySelectScene::handleBackButton(const PointerEvent& event) {
    const bool inside = hitTest(event, kBackCx - kBackRadius, kBackCy - kBackRadius,
                                kBackRadius * 2.0F, kBackRadius * 2.0F);
    if (event.phase == PointerEvent::Phase::Down) {
        backPressed_ = inside;
        return inside;
    }
    const bool wasPressed = backPressed_;
    backPressed_ = false;
    if (wasPressed && inside) {
        manager_.pop();
        return true;
    }
    return false;
}

void DifficultySelectScene::handleInput(const PointerEvent& event) {
    if (handleBackButton(event)) {
        return;
    }
    // Tap anywhere on the (generously sized) track to snap the knob.
    if (event.phase == PointerEvent::Phase::Down &&
        hitTest(event, kTrackX - 40.0F, kTrackCy - 50.0F, kTrackW + 80.0F, 100.0F)) {
        const float t = std::clamp((event.x - kTrackX) / kTrackW, 0.0F, 1.0F);
        const auto stop = static_cast<int>(std::lround(t * 2.0F)); // 0, 1, or 2
        setDifficulty(static_cast<Difficulty>(stop));
        return;
    }
    playButton_.handleInput(event);
}

void DifficultySelectScene::update(float /*dtSeconds*/) {}

void DifficultySelectScene::render(Canvas& canvas) {
    canvas.clear(colors::cream);

    // Back button.
    canvas.fillCircle(kBackCx, kBackCy, kBackRadius, colors::white);
    canvas.line(kBackCx + 12.0F, kBackCy - 24.0F, kBackCx - 14.0F, kBackCy, 14.0F, kChevron);
    canvas.line(kBackCx - 14.0F, kBackCy, kBackCx + 12.0F, kBackCy + 24.0F, 14.0F, kChevron);

    canvas.textCentered(titleUpper_, layout::kWidthF / 2.0F, 150.0F, 64.0F, colors::gridBlack);

    if (descLines_.empty()) {
        descLines_ = wrapText(canvas, info_.description, 30.0F, kDescMaxWidth);
    }
    float descY = 240.0F;
    for (const std::string& descLine : descLines_) {
        canvas.textCentered(descLine, layout::kWidthF / 2.0F, descY, 30.0F, kDescColor);
        descY += 44.0F;
    }

    canvas.textCentered(faceFor(difficulty_), layout::kWidthF / 2.0F, 560.0F, 150.0F,
                        color(difficulty_));
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, 710.0F, 76.0F,
                        color(difficulty_));

    // Slider: background track, coloured fill up to the knob, then the knob.
    canvas.fillRoundedRect(kTrackX, kTrackCy - (kTrackH / 2.0F), kTrackW, kTrackH, kTrackH / 2.0F,
                           kTrackBg);
    const float knobX = stopX(difficulty_);
    canvas.fillRoundedRect(kTrackX, kTrackCy - (kTrackH / 2.0F), knobX - kTrackX, kTrackH,
                           kTrackH / 2.0F, color(difficulty_));
    canvas.fillCircle(knobX, kTrackCy, kKnobRadius, colors::white);
    canvas.fillCircle(knobX, kTrackCy, kKnobRadius - 8.0F, color(difficulty_));

    playButton_.render(canvas);
}

} // namespace og
