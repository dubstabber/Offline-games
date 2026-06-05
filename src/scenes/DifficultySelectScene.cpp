#include "scenes/DifficultySelectScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Theme.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace og {
namespace {

// ---- Back button (circular, top-left) -----------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

// ---- Slider --------------------------------------------------------------
constexpr float kTrackX = 160.0F;
constexpr float kTrackW = 400.0F;
constexpr float kTrackCy = 820.0F;
constexpr float kTrackH = 28.0F;
constexpr float kKnobRadius = 34.0F;

// ---- PLAY button ---------------------------------------------------------
constexpr float kPlayW = 400.0F;
constexpr float kPlayH = 130.0F;
constexpr float kPlayX = (layout::kWidthF - kPlayW) / 2.0F;
constexpr float kPlayY = 1020.0F;

constexpr float kDescMaxWidth = 600.0F;

const char* faceFor(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return "\xF0\x9F\x99\x82"; // 🙂
    case Difficulty::Medium:
        return "\xF0\x9F\x98\x90"; // 😐
    case Difficulty::Hard:
        return "\xF0\x9F\x98\x88"; // 😈
    case Difficulty::VeryHard:
        return "\xF0\x9F\xA4\xAF"; // 🤯
    }
    return "";
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
    : manager_(manager), info_(std::move(info)),
      stops_(std::clamp(info_.difficultyCount, 2, 4)), // Difficulty has at most 4 values
      titleUpper_(info_.title),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kBackRadius),
      playButton_("PLAY", kPlayX, kPlayY, kPlayW, kPlayH) {
    backButton_.setOnTap([this] { manager_.pop(); });
    std::ranges::transform(titleUpper_, titleUpper_.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    setDifficulty(difficulty_);
    playButton_.setOnTap([this] { manager_.push(info_.create(manager_, difficulty_)); });
    if (info_.currentLevel) {
        // PLAY and the "Level N" subtitle are drawn as two lines in render(), so
        // the button itself draws only its fill (empty label).
        playButton_.setLabel("");
    }
}

void DifficultySelectScene::setDifficulty(Difficulty difficulty) {
    difficulty_ = difficulty;
    playButton_.setColors(color(difficulty_), colors::white);
}

void DifficultySelectScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (handleSlider(event)) {
        return;
    }
    playButton_.handleInput(event);
}

// Drag the knob along the track. A press on the (generously sized) track starts
// a drag; subsequent moves slide the knob under the finger; release ends it.
// While dragging, the difficulty snaps live to the nearest of the three stops.
bool DifficultySelectScene::handleSlider(const PointerEvent& event) {
    constexpr float kHitX = kTrackX - 40.0F;
    constexpr float kHitW = kTrackW + 80.0F;
    constexpr float kHitY = kTrackCy - 50.0F;
    constexpr float kHitH = 100.0F;
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        if (hitTest(event, kHitX, kHitY, kHitW, kHitH)) {
            draggingKnob_ = true;
            dragKnobTo(event.x);
            return true;
        }
        return false;
    case PointerEvent::Phase::Move:
        if (draggingKnob_) {
            dragKnobTo(event.x);
            return true;
        }
        return false;
    case PointerEvent::Phase::Up:
        if (draggingKnob_) {
            draggingKnob_ = false;
            return true;
        }
        return false;
    }
    return false;
}

void DifficultySelectScene::dragKnobTo(float x) {
    knobX_ = std::clamp(x, kTrackX, kTrackX + kTrackW);
    const float t = (knobX_ - kTrackX) / kTrackW;
    const auto stop = static_cast<int>(std::lround(t * static_cast<float>(stops_ - 1)));
    setDifficulty(static_cast<Difficulty>(std::clamp(stop, 0, stops_ - 1)));
}

// Knob centre x for a stop, spread evenly across the track (Easy = left edge,
// the hardest = right edge), so 3- and 4-difficulty games space their stops out.
float DifficultySelectScene::stopX(Difficulty difficulty) const {
    const float frac =
        static_cast<float>(static_cast<int>(difficulty)) / static_cast<float>(stops_ - 1);
    return kTrackX + (kTrackW * frac);
}

void DifficultySelectScene::update(float /*dtSeconds*/) {}

void DifficultySelectScene::render(Canvas& canvas) {
    canvas.clear(theme().menuBg);

    backButton_.render(canvas);

    canvas.textCentered(titleUpper_, layout::kWidthF / 2.0F, 150.0F, 64.0F, theme().titleText);

    if (descLines_.empty()) {
        descLines_ = wrapText(canvas, info_.description, 30.0F, kDescMaxWidth);
    }
    float descY = 240.0F;
    for (const std::string& descLine : descLines_) {
        canvas.textCentered(descLine, layout::kWidthF / 2.0F, descY, 30.0F, theme().mutedText);
        descY += 44.0F;
    }

    canvas.textCentered(faceFor(difficulty_), layout::kWidthF / 2.0F, 560.0F, 150.0F,
                        color(difficulty_));
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, 710.0F, 76.0F,
                        color(difficulty_));

    // Slider: background track, coloured fill up to the knob, then the knob.
    canvas.fillRoundedRect(kTrackX, kTrackCy - (kTrackH / 2.0F), kTrackW, kTrackH, kTrackH / 2.0F,
                           theme().sliderTrack);
    const float knobX = draggingKnob_ ? knobX_ : stopX(difficulty_);
    canvas.fillRoundedRect(kTrackX, kTrackCy - (kTrackH / 2.0F), knobX - kTrackX, kTrackH,
                           kTrackH / 2.0F, color(difficulty_));
    canvas.fillCircle(knobX, kTrackCy, kKnobRadius, colors::white);
    canvas.fillCircle(knobX, kTrackCy, kKnobRadius - 8.0F, color(difficulty_));

    playButton_.render(canvas);
    // For level-tracked games (e.g. Tap Match), draw "PLAY" over a "Level N"
    // subtitle inside the button — matching the original's difficulty screen.
    if (info_.currentLevel) {
        const float cx = layout::kWidthF / 2.0F;
        canvas.textCentered("PLAY", cx, kPlayY + (kPlayH * 0.38F), kPlayH * 0.40F, colors::white);
        const std::string levelText = "Level " + std::to_string(info_.currentLevel(difficulty_));
        canvas.textCentered(levelText, cx, kPlayY + (kPlayH * 0.74F), kPlayH * 0.20F,
                            colors::white);
    }
}

} // namespace og
