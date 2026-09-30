# Test executable - links the shared core library (no recompiled sources).
# template_renderer.cpp is compiled in here: it lives in the UMLGenerator
# target, not the core library, and the splice test needs it.
add_executable(code_analyzer_tests
    gtest/main_test.cpp
    ${CMAKE_SOURCE_DIR}/src/uml/template_renderer.cpp
    gtest/registry_test.cpp
    gtest/provider_test.cpp
    gtest/observer_test.cpp
    gtest/config_test.cpp
    gtest/uml_test.cpp
    gtest/csharp_test.cpp
    gtest/cpp_kind_test.cpp
)

# Tests locate shared fixtures (test_files/) relative to the source root
target_compile_definitions(code_analyzer_tests PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# main_test.cpp provides the test entry point, so link gtest (not gtest_main).
target_link_libraries(code_analyzer_tests
    code_analyzer_core
    GTest::gtest
)

# Add test
add_test(NAME code_analyzer_tests COMMAND code_analyzer_tests)
