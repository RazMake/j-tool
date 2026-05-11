cmake_minimum_required(VERSION 3.20)
project(Jump C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use static runtime (/MT instead of /MD)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Warning level
if(MSVC)
    add_compile_options(/W4)
endif()

# --- j_lib: static library with all logic ---
set(J_LIB_SOURCES
    src/config/ini_parser.c
    src/config/cache.c
    src/config/config.c
    src/resolve/resolver.c
    src/resolve/suggest.c
    src/platform/tc.c
    src/platform/osd.c
    src/platform/install.c
    src/core/jump.c
)

add_library(j_lib STATIC ${J_LIB_SOURCES})
target_include_directories(j_lib PUBLIC src/core src/config src/resolve src/platform)
target_link_libraries(j_lib PRIVATE advapi32 shell32 user32 gdi32)

# --- jc: console executable ---
add_executable(jc src/core/main.c)
target_link_libraries(jc PRIVATE j_lib)

# --- j: windows subsystem executable ---
add_executable(j WIN32 src/core/main_win.c)
target_link_libraries(j PRIVATE j_lib)

# --- Testing ---
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        cmocka
        GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
        GIT_TAG        cmocka-1.1.7
        GIT_SHALLOW    TRUE
    )
    set(WITH_STATIC_LIB   ON  CACHE BOOL "" FORCE)
    set(WITH_CMOCKA_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_EXAMPLES     OFF CACHE BOOL "" FORCE)
    set(UNIT_TESTING      ON  CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(cmocka)

    set(TEST_SOURCES
        tests/test_main.c
        tests/config/test_ini_parser.c
        tests/config/test_cache.c
        tests/config/test_config.c
        tests/resolve/test_resolver.c
        tests/resolve/test_suggest.c
        tests/platform/test_tc.c
        tests/platform/test_install.c
    )

    add_executable(j_tests ${TEST_SOURCES})
    target_link_libraries(j_tests PRIVATE j_lib cmocka-static)
    target_include_directories(j_tests PRIVATE
        src/core src/config src/resolve src/platform
        ${cmocka_SOURCE_DIR}/include)

    # Make fixture path available to tests
    set(FIXTURES_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures")
    string(REPLACE "\\" "/" FIXTURES_DIR "${FIXTURES_DIR}")
    target_compile_definitions(j_tests PRIVATE "FIXTURES_DIR=\"${FIXTURES_DIR}\"")

    add_test(NAME jump_tests COMMAND j_tests)
endif()
