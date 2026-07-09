cmake_minimum_required(VERSION 3.23)

project(FrrBgpdTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED)
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
    GIT_REPOSITORY  git@github.com:nlohmann/json.git
    GIT_TAG         55f93686c01528224f448c19128836e7df245f72 # release-3.12.0
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
FetchContent_MakeAvailable(json)

add_executable(model_tests "src/main.cpp" "src/test_bgp_ls_edge_update.cpp")
target_include_directories(model_tests PUBLIC "{CMAKE_SOURCE_DIR}/include")
target_link_libraries(model_tests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)


include(GoogleTest)
gtest_discover_tests(model_tests)
