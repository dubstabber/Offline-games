cmake_minimum_required(VERSION 3.20)
project(offline-games LANGUAGES CXX)

# ---- Standard ----------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Warnings ----------------------------------------------------------------
# Strict by default; the `dev` preset turns these into errors via
# WARNINGS_AS_ERRORS. Flags are compiler-specific so MSVC builds still work.
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4 /permissive-)
    if(WARNINGS_AS_ERRORS)
        target_compile_options(project_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference)
    if(WARNINGS_AS_ERRORS)
        target_compile_options(project_warnings INTERFACE -Werror)
    endif()
endif()

# ---- Dependencies ------------------------------------------------------------
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

# ---- Engine library (everything except main, so tests can link it) -----------
add_library(engine STATIC
    src/core/App.cpp
    src/core/SceneManager.cpp
    src/core/FontManager.cpp
    src/core/Canvas.cpp
    src/core/Settings.cpp
    src/core/Theme.cpp
    src/ui/Button.cpp
    src/ui/Label.cpp
    src/ui/Toggle.cpp
    src/ui/Slider.cpp
    src/games/GameRegistry.cpp
    src/games/tictactoe/TicTacToeBoard.cpp
    src/games/tictactoe/TicTacToeBot.cpp
    src/games/tictactoe/TicTacToeScene.cpp
    src/games/tapmatch/TapMatchBoard.cpp
    src/games/tapmatch/TapMatchLevels.cpp
    src/games/tapmatch/TapMatchScene.cpp
    src/games/minesweeper/MineSweeperBoard.cpp
    src/games/minesweeper/MineSweeperSolver.cpp
    src/games/minesweeper/MineSweeperScene.cpp
    src/games/blockfill/BlockFillBoard.cpp
    src/games/blockfill/BlockFillLevels.cpp
    src/games/blockfill/BlockFillScene.cpp
    src/scenes/DifficultySelectScene.cpp
    src/scenes/MenuScene.cpp
    src/scenes/SettingsScene.cpp)
target_include_directories(engine PUBLIC src)
target_link_libraries(engine
    PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf
    PRIVATE project_warnings)

# ---- Executable --------------------------------------------------------------
add_executable(offline-games src/main.cpp)
target_link_libraries(offline-games PRIVATE engine project_warnings)

# Ship the fonts next to the executable so SDL_GetBasePath() finds them on every
# OS without relying on system-installed fonts. FontManager falls back to system
# locations if this bundle is missing.
add_custom_command(TARGET offline-games POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/fonts"
            "$<TARGET_FILE_DIR:offline-games>/assets/fonts"
    COMMENT "Bundling fonts next to the executable")

# Ship the Tap Match level data the same way: a binary blob of every original
# board, parsed at runtime (see games/tapmatch/TapMatchLevels). It is data, not
# an image, so the "no image assets" rule does not apply.
add_custom_command(TARGET offline-games POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/tapmatch"
            "$<TARGET_FILE_DIR:offline-games>/assets/tapmatch"
    COMMENT "Bundling Tap Match levels next to the executable")

# Ship the Block Fill level data the same way: a binary blob of original boards
# split into four difficulty pools (see games/blockfill/BlockFillLevels). Data,
# not an image, so the "no image assets" rule does not apply.
add_custom_command(TARGET offline-games POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/blockfill"
            "$<TARGET_FILE_DIR:offline-games>/assets/blockfill"
    COMMENT "Bundling Block Fill levels next to the executable")

# ---- Tests -------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
