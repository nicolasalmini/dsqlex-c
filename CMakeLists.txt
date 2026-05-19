cmake_minimum_required(VERSION 3.20)
project(dsqlex VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Find mpdecimal -----------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPDEC REQUIRED libmpdec)
pkg_check_modules(MPDECXX REQUIRED libmpdec++)

# Static library paths for mpdecimal (to avoid symbol conflicts with Python)
set(MPDEC_STATIC_LIB "${MPDEC_LIBRARY_DIRS}/libmpdec.a")
set(MPDECXX_STATIC_LIB "${MPDEC_LIBRARY_DIRS}/libmpdec++.a")

# --- Core library (C++ internals) --------------------------------------------
add_library(dsqlex_core STATIC
    src/lexer.cpp
    src/parser.cpp
    src/evaluator.cpp
)
target_include_directories(dsqlex_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC  ${MPDECXX_INCLUDE_DIRS}
)
# Force-link static .a files to avoid dynamic symbol conflicts with Python's mpdecimal
target_link_libraries(dsqlex_core PUBLIC ${MPDECXX_STATIC_LIB} ${MPDEC_STATIC_LIB})

# --- Shared library with C API ------------------------------------------------
add_library(dsqlex SHARED src/dsqlex_c_api.cpp)
target_link_libraries(dsqlex PRIVATE dsqlex_core)
target_include_directories(dsqlex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Hide all symbols except our C API to prevent mpdecimal symbol conflicts
set_target_properties(dsqlex PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION 0
    OUTPUT_NAME dsqlex
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# --- Tests --------------------------------------------------------------------
option(DSQLEX_BUILD_TESTS "Build tests" ON)
if(DSQLEX_BUILD_TESTS)
    enable_testing()
    add_executable(dsqlex_tests tests/main.cpp)
    target_link_libraries(dsqlex_tests PRIVATE dsqlex_core dsqlex)
    add_test(NAME dsqlex_tests COMMAND dsqlex_tests)
endif()
