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
