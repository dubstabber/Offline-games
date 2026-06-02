# Unit tests for the pure (SDL-free) game logic. Kept as a plain assert-based
# executable registered with CTest — no test framework dependency to install.
add_executable(test_tictactoe test_tictactoe.cpp)
target_link_libraries(test_tictactoe PRIVATE engine)
add_test(NAME tictactoe COMMAND test_tictactoe)
