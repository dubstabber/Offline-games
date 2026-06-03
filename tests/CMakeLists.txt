# Unit tests for the pure (SDL-free) game logic. Kept as a plain assert-based
# executable registered with CTest — no test framework dependency to install.
add_executable(test_tictactoe test_tictactoe.cpp)
target_link_libraries(test_tictactoe PRIVATE engine)
add_test(NAME tictactoe COMMAND test_tictactoe)

add_executable(test_tictactoe_bot test_tictactoe_bot.cpp)
target_link_libraries(test_tictactoe_bot PRIVATE engine)
add_test(NAME tictactoe_bot COMMAND test_tictactoe_bot)

add_executable(test_tapmatch test_tapmatch.cpp)
target_link_libraries(test_tapmatch PRIVATE engine)
add_test(NAME tapmatch COMMAND test_tapmatch)

# Decodes the real bundled level blob and asserts every level is well-formed and
# solvable, so the asset path is passed in as a compile definition.
add_executable(test_tapmatch_levels test_tapmatch_levels.cpp)
target_link_libraries(test_tapmatch_levels PRIVATE engine)
target_compile_definitions(test_tapmatch_levels PRIVATE
    TAPMATCH_LEVELS_PATH="${CMAKE_SOURCE_DIR}/assets/tapmatch/levels.bytes")
add_test(NAME tapmatch_levels COMMAND test_tapmatch_levels)

add_executable(test_settings test_settings.cpp)
target_link_libraries(test_settings PRIVATE engine)
add_test(NAME settings COMMAND test_settings)

add_executable(test_minesweeper test_minesweeper.cpp)
target_link_libraries(test_minesweeper PRIVATE engine)
add_test(NAME minesweeper COMMAND test_minesweeper)
