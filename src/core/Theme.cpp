#include "core/Theme.hpp"

#include "core/Settings.hpp"

namespace og {

const Theme& theme() {
    return settings().darkMode ? kDark : kLight;
}

} // namespace og
