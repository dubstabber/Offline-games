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

# Header-only utility checks (Easing, GridLayout, FixedTimestep). They link engine
# only to reuse its include path; the units under test are header-only.
add_executable(test_easing test_easing.cpp)
target_link_libraries(test_easing PRIVATE engine)
add_test(NAME easing COMMAND test_easing)

add_executable(test_gridlayout test_gridlayout.cpp)
target_link_libraries(test_gridlayout PRIVATE engine)
add_test(NAME gridlayout COMMAND test_gridlayout)

add_executable(test_minesweeper test_minesweeper.cpp)
target_link_libraries(test_minesweeper PRIVATE engine)
add_test(NAME minesweeper COMMAND test_minesweeper)

# Exercises the pure board logic and the level parser; also decodes the real
# bundled blob and asserts every board is well-formed, so the asset path is
# passed in as a compile definition.
add_executable(test_blockfill test_blockfill.cpp)
target_link_libraries(test_blockfill PRIVATE engine)
target_compile_definitions(test_blockfill PRIVATE
    BLOCKFILL_LEVELS_PATH="${CMAKE_SOURCE_DIR}/assets/blockfill/levels.bytes")
add_test(NAME blockfill COMMAND test_blockfill)

add_executable(test_sokoban test_sokoban.cpp)
target_link_libraries(test_sokoban PRIVATE engine)
target_compile_definitions(test_sokoban PRIVATE
    SOKOBAN_LEVELS_PATH="${CMAKE_SOURCE_DIR}/assets/sokoban/levels.bytes")
add_test(NAME sokoban COMMAND test_sokoban)

add_executable(test_nibbles test_nibbles.cpp)
target_link_libraries(test_nibbles PRIVATE engine)
target_compile_definitions(test_nibbles PRIVATE
    NIBBLES_LEVELS_PATH="${CMAKE_SOURCE_DIR}/assets/nibbles/levels.bytes")
add_test(NAME nibbles COMMAND test_nibbles)

add_executable(test_snake test_snake.cpp)
target_link_libraries(test_snake PRIVATE engine)
add_test(NAME snake COMMAND test_snake)

add_executable(test_hexanaut test_hexanaut.cpp)
target_link_libraries(test_hexanaut PRIVATE engine)
add_test(NAME hexanaut COMMAND test_hexanaut)

add_executable(test_hole test_hole.cpp)
target_link_libraries(test_hole PRIVATE engine)
add_test(NAME hole COMMAND test_hole)

add_executable(test_twentyfortyeight test_twentyfortyeight.cpp)
target_link_libraries(test_twentyfortyeight PRIVATE engine)
add_test(NAME twentyfortyeight COMMAND test_twentyfortyeight)

add_executable(test_memory test_memory.cpp)
target_link_libraries(test_memory PRIVATE engine)
add_test(NAME memory COMMAND test_memory)

add_executable(test_memory_bot test_memory_bot.cpp)
target_link_libraries(test_memory_bot PRIVATE engine)
add_test(NAME memory_bot COMMAND test_memory_bot)

add_executable(test_klondike test_klondike.cpp)
target_link_libraries(test_klondike PRIVATE engine)
add_test(NAME klondike COMMAND test_klondike)

add_executable(test_klondike_solver test_klondike_solver.cpp)
target_link_libraries(test_klondike_solver PRIVATE engine)
add_test(NAME klondike_solver COMMAND test_klondike_solver)

# Parses the shipped deal lists and re-solves the cheapest seed of each tier, so
# the asset path is passed in as a compile definition.
add_executable(test_solitaire_deals test_solitaire_deals.cpp)
target_link_libraries(test_solitaire_deals PRIVATE engine)
target_compile_definitions(test_solitaire_deals PRIVATE
    SOLITAIRE_DEALS_PATH="${CMAKE_SOURCE_DIR}/assets/solitaire/deals.bytes")
add_test(NAME solitaire_deals COMMAND test_solitaire_deals)

add_executable(test_arrows test_arrows.cpp)
target_link_libraries(test_arrows PRIVATE engine)
add_test(NAME arrows COMMAND test_arrows)
