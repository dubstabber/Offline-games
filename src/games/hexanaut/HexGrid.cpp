#include "games/hexanaut/HexGrid.hpp"

namespace og::hexanaut {

HexGrid::HexGrid(int width, int height)
    : width_(width), height_(height),
      cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

} // namespace og::hexanaut
