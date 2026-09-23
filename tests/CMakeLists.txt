# Test executable - separate from main project
add_executable(code_analyzer_tests
    gtest/main_test.cpp
    gtest/uml_test.cpp
)

# Link with gtest
target_link_libraries(code_analyzer_tests
    GTest::gtest
    GTest::gtest_main
)

# Add test
add_test(NAME code_analyzer_tests COMMAND code_analyzer_tests)