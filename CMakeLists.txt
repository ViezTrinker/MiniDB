cmake_minimum_required(VERSION 3.24)

project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
   SFML
   GIT_REPOSITORY https://github.com/SFML/SFML.git
   GIT_TAG 3.1.0
   GIT_SHALLOW ON
   EXCLUDE_FROM_ALL
   SYSTEM
)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
   googletest
   GIT_REPOSITORY https://github.com/google/googletest.git
   GIT_TAG v1.15.2
   GIT_SHALLOW ON
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
   nlohmann_json
   GIT_REPOSITORY https://github.com/nlohmann/json.git
   GIT_TAG v3.11.3
   GIT_SHALLOW ON
)
FetchContent_MakeAvailable(nlohmann_json)

set(MINIDB_CORE_HEADERS
   src/ai/ai_actions.h
   src/ai/ai_observation.h
   src/ai/ai_planner.h
   src/ai/ai_scorer.h
   src/ai/play_agent.h
   src/core/constants.h
   src/core/result.h
   src/core/types.h
   src/data/station_catalog.h
   src/geo/geojson_outline.h
   src/geo/projection.h
   src/simulation/gravity_model.h
   src/simulation/network.h
   src/simulation/passenger.h
   src/simulation/pathfinder.h
   src/simulation/train.h
   src/simulation/economy.h
   src/simulation/play_session_log.h
   src/simulation/track_inventory.h
   src/simulation/world.h
)

set(MINIDB_CORE_SOURCES
   src/ai/ai_observation.cpp
   src/ai/ai_planner.cpp
   src/ai/ai_scorer.cpp
   src/ai/play_agent.cpp
   src/data/station_catalog.cpp
   src/geo/geojson_outline.cpp
   src/geo/projection.cpp
   src/simulation/economy.cpp
   src/simulation/gravity_model.cpp
   src/simulation/network.cpp
   src/simulation/passenger.cpp
   src/simulation/pathfinder.cpp
   src/simulation/play_session_log.cpp
   src/simulation/track_inventory.cpp
   src/simulation/train.cpp
   src/simulation/world.cpp
)

add_library(MiniDbCore STATIC
   ${MINIDB_CORE_SOURCES}
   ${MINIDB_CORE_HEADERS}
)
target_include_directories(MiniDbCore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(MiniDbCore PUBLIC nlohmann_json::nlohmann_json)

set(MINIDB_APP_HEADERS
   src/application/game.h
   src/application/main_menu.h
   src/input/line_editor.h
   src/rendering/renderer.h
   src/rendering/utf8_text.h
)

set(MINIDB_APP_SOURCES
   src/main.cpp
   src/application/game.cpp
   src/application/main_menu.cpp
   src/input/line_editor.cpp
   src/rendering/renderer.cpp
)

add_executable(MiniDB
   ${MINIDB_APP_SOURCES}
   ${MINIDB_APP_HEADERS}
)
target_include_directories(MiniDB PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(MiniDB PRIVATE MiniDbCore SFML::Graphics)

if(WIN32)
   target_link_libraries(MiniDB PRIVATE SFML::Main)
endif()

set(MINIDB_TEST_SOURCES
   tests/economy_test.cpp
   tests/geojson_outline_test.cpp
   tests/gravity_model_test.cpp
   tests/line_editor_test.cpp
   tests/network_test.cpp
   tests/pathfinder_test.cpp
   tests/play_session_log_test.cpp
   tests/projection_test.cpp
   tests/station_catalog_test.cpp
   tests/train_test.cpp
   tests/world_test.cpp
   tests/ai_planner_test.cpp
   src/input/line_editor.cpp
   src/input/line_editor.h
)

add_executable(MiniDBTests
   ${MINIDB_TEST_SOURCES}
   tests/test_helpers.h
)
target_include_directories(MiniDBTests PRIVATE
   ${CMAKE_SOURCE_DIR}/src
   ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(MiniDBTests PRIVATE MiniDbCore GTest::gtest_main)
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" MINIDB_SOURCE_DIR_NORMALIZED)
target_compile_definitions(MiniDBTests PRIVATE
   MINIDB_SOURCE_DIR="${MINIDB_SOURCE_DIR_NORMALIZED}"
)

if(MSVC)
   target_compile_options(MiniDbCore PRIVATE /utf-8 /W4)
   target_compile_options(MiniDB PRIVATE /utf-8 /W4)
   target_compile_options(MiniDBTests PRIVATE /utf-8 /W4)
endif()

add_custom_command(TARGET MiniDB POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:MiniDB>/data
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(MiniDBTests
   DISCOVERY_MODE PRE_TEST
   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
