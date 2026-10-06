# Test executable - links the shared libraries (no recompiled core/UML sources).
# uml_server.cpp is compiled in here: it lives only in the UmlServer executable
# target, not a library, and the in-process server test needs the UmlServer class.
add_executable(code_analyzer_tests
    gtest/main_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/uml_server.cpp
    ${CMAKE_SOURCE_DIR}/src/server/review_store.cpp
    ${CMAKE_SOURCE_DIR}/src/server/http_util.cpp
    ${CMAKE_SOURCE_DIR}/src/server/rest_handlers.cpp
    ${CMAKE_SOURCE_DIR}/src/server/source_resolver.cpp
    ${CMAKE_SOURCE_DIR}/src/server/openapi.cpp
    ${CMAKE_SOURCE_DIR}/src/server/class_index.cpp
    gtest/registry_test.cpp
    gtest/provider_test.cpp
    gtest/observer_test.cpp
    gtest/config_test.cpp
    gtest/uml_test.cpp
    gtest/uml_model_test.cpp
    gtest/uml_server_test.cpp
    gtest/openapi_spec_test.cpp
    gtest/render_route_test.cpp
    gtest/cmake_analyzer_test.cpp
    gtest/thread_pool_test.cpp
    gtest/csharp_test.cpp
    gtest/cpp_kind_test.cpp
    gtest/source_file_fix_test.cpp
    gtest/call_graph_test.cpp
    gtest/class_index_test.cpp
)

# Tests locate shared fixtures (test_files/) relative to the source root
target_compile_definitions(code_analyzer_tests PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# main_test.cpp provides the test entry point, so link gtest (not gtest_main).
# code_analyzer_uml provides the class-loading/splicing/UmlModel facade (and,
# transitively, the src/ include root + Boost::json) for the UML tests;
# code_analyzer_uml also carries the Boost include dir uml_server.cpp needs.
target_link_libraries(code_analyzer_tests
    code_analyzer_core
    code_analyzer_uml
    GTest::gtest
)

# Add test
add_test(NAME code_analyzer_tests COMMAND code_analyzer_tests)
