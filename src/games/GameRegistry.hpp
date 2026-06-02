#pragma once

#include "games/GameInfo.hpp"

#include <vector>

namespace og {

// The catalog of all games shown in the menu, in display order.
const std::vector<GameInfo>& gameRegistry();

} // namespace og
