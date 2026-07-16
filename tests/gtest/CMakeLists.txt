cmake_minimum_required(VERSION 3.23)

project(FrrBgpdTests LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

include(FetchContent)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         52eb8108c5bdec04579160ae17225d66034bd723 # release-1.17.0
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    json
    GIT_REPOSITORY  https://github.com/nlohmann/json.git
    GIT_TAG         55f93686c01528224f448c19128836e7df245f72 # release-3.12.0
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
FetchContent_MakeAvailable(json)

find_library(LIBFRR
    NAMES libfrr.a
    HINTS /lib
    REQUIRED
)

find_library(LIBBGP
    NAMES libbgp.a
    HINTS /home/frr/frr/bgpd
    REQUIRED
)

find_library(LIBRFP
    NAMES librfp.a
    HINTS /home/frr/frr/bgpd/rfp-example/librfp
    REQUIRED
)

add_executable(model_tests 
    "src/main.cpp" 
    "src/test_bgp_ls_edge_update.cpp" 
    "src/bgpd_env.cpp"
    "src/frr_bridge.c"
    "src/sbuf.c"
)

set_target_properties(model_tests PROPERTIES POSITION_INDEPENDENT_CODE OFF)

target_link_libraries(model_tests
    PRIVATE
        GTest::gtest_main
        nlohmann_json::nlohmann_json

        -Wl,--start-group
        ${LIBFRR}
        ${LIBBGP}
        ${LIBRFP}
        -Wl,--end-group

        json-c
        sqlite3
        yang
        cap
        crypt
        m
        gcov
        lua5.3
)
target_include_directories(model_tests
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "/home/frr/frr"
        "/home/frr/frr/lib"
)

target_compile_options(model_tests PRIVATE -DHAVE_CONFIG_H)
target_compile_definitions(model_tests PRIVATE "MODEL_JSON_PATH=\"${CMAKE_SOURCE_DIR}/tests/edge_update_formatted.json\"")
