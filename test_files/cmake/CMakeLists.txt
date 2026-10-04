# CMake-graph fixture: exercises STATIC/SHARED/INTERFACE libraries, an ALIAS,
# target_sources, and target_link_libraries with keywords, generator
# expressions, variables, and -l flags that the parser must filter out.
cmake_minimum_required(VERSION 3.16)
project(cmake_fixture CXX)

add_library(core STATIC core.cpp core.h)
target_sources(core PUBLIC common.h)
target_link_libraries(core PUBLIC util)

add_library(net SHARED net.cpp)
target_link_libraries(net PRIVATE core)

add_library(iface INTERFACE)
target_link_libraries(iface INTERFACE net)

add_library(core-alias ALIAS core)

add_executable(app app.cpp)
target_link_libraries(app
    PRIVATE net util ${CMAKE_DL_LIBS} -lz $<TARGET_NAME:core>)
