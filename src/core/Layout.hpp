#pragma once

namespace og::layout {

// All scenes draw in this fixed logical coordinate space (a 1:2 portrait canvas
// matching the PinePhone's 720x1440 panel). SDL's logical presentation scales
// and letterboxes it to whatever the real window/screen size is, so layouts are
// resolution-independent: the same code fills the phone screen and a desktop
// dev window. Coordinates are in "logical pixels".
inline constexpr int kWidth = 720;
inline constexpr int kHeight = 1440;

inline constexpr float kWidthF = static_cast<float>(kWidth);
inline constexpr float kHeightF = static_cast<float>(kHeight);

// Minimum comfortable touch target (~9mm on the PinePhone panel).
inline constexpr float kMinTouchSize = 96.0F;

} // namespace og::layout
