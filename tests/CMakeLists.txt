# Test executable - separate from main project
add_executable(code_analyzer_tests
    gtest/main_test.cpp
    gtest/uml_test.cpp
    gtest/csharp_test.cpp
    ../src/parser/cpp_parser.cpp
    ../src/parser/csharp_parser.cpp
    ../src/parser/python_parser.cpp
    ../src/core/analyzer.cpp
    ../src/core/model.cpp
)

# Link with gtest
target_link_libraries(code_analyzer_tests
    GTest::gtest
    GTest::gtest_main
    Boost::json
)

# Add test
add_test(NAME code_analyzer_tests COMMAND code_analyzer_tests)