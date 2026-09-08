cmake_minimum_required(VERSION 3.28)
project(marpg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(FetchContent)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

# tmxlite's CMake is older; CMake 4.x needs this for subprojects
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(TMXLITE_STATIC_LIB TRUE CACHE BOOL "" FORCE)

FetchContent_Declare(tmxlite
    GIT_REPOSITORY https://github.com/fallahn/tmxlite.git
    GIT_TAG v1.4.4
    GIT_SHALLOW ON
    SOURCE_SUBDIR tmxlite
)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
)

FetchContent_MakeAvailable(SFML tmxlite json)

add_library(common STATIC
    common/collision_world.cpp
    common/settings.cpp
    common/common.cpp
    common/logger.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(common PUBLIC
    SFML::System
    SFML::Network
    SFML::Graphics
    tmxlite
)

add_executable(server server/server.cpp)
target_link_libraries(server PRIVATE
    common
    SFML::System
    SFML::Network
)

add_executable(client client/client.cpp client/input_manager.cpp client/player.cpp client/camera.cpp client/client_connection.cpp client/resource_manager.cpp client/hud.cpp)
target_link_libraries(client PRIVATE
    common
    SFML::System
    SFML::Window
    SFML::Graphics
    SFML::Network
    SFML::Audio
    nlohmann_json::nlohmann_json
)
include(CTest)
if(BUILD_TESTING)
    add_executable(shutdown_probe tests/shutdown_probe.cpp client/client_connection.cpp)
    target_link_libraries(shutdown_probe PRIVATE common)
    add_executable(kill_feed_tests tests/kill_feed_tests.cpp)
    target_link_libraries(kill_feed_tests PRIVATE common)
    add_test(NAME kill_feed COMMAND kill_feed_tests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(bot_ai_tests tests/bot_ai_tests.cpp)
    target_link_libraries(bot_ai_tests PRIVATE common)
    add_test(NAME bot_ai COMMAND bot_ai_tests ${CMAKE_CURRENT_SOURCE_DIR}/assets/tiled/Demo.tmx)
    add_executable(connection_probe tests/connection_probe.cpp client/client_connection.cpp)
    target_link_libraries(connection_probe PRIVATE common)
    add_executable(settings_tests tests/settings_tests.cpp)
    target_link_libraries(settings_tests PRIVATE common)
    add_test(NAME settings COMMAND settings_tests ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(world_transport_tests tests/world_transport_tests.cpp)
    target_link_libraries(world_transport_tests PRIVATE common)
    add_test(NAME world_transport COMMAND world_transport_tests)
    add_executable(sound_tests tests/sound_tests.cpp)
    target_link_libraries(sound_tests PRIVATE common SFML::Audio)
    add_test(NAME sound COMMAND sound_tests ${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/Retro_Combat_FX)
    add_executable(collision_tests tests/collision_tests.cpp)
    target_link_libraries(collision_tests PRIVATE common)
    add_executable(combat_tests tests/combat_tests.cpp)
    target_link_libraries(combat_tests PRIVATE common)
    add_test(NAME combat COMMAND combat_tests)
    add_executable(damage_number_tests tests/damage_number_tests.cpp)
    target_link_libraries(damage_number_tests PRIVATE common)
    add_test(NAME damage_numbers COMMAND damage_number_tests)
    add_executable(trigger_tests tests/trigger_tests.cpp)
    target_link_libraries(trigger_tests PRIVATE common)
    add_test(NAME triggers COMMAND trigger_tests ${CMAKE_CURRENT_SOURCE_DIR}/assets/tiled/Demo.tmx)
    add_executable(occlusion_tests tests/occlusion_tests.cpp)
    target_link_libraries(occlusion_tests PRIVATE common)
    add_test(NAME occlusion COMMAND occlusion_tests ${CMAKE_CURRENT_SOURCE_DIR}/shaders/player_occlusion.frag ${CMAKE_CURRENT_SOURCE_DIR}/assets/tiled/Demo.tmx)
    add_executable(demo_level_tests tests/demo_level_tests.cpp)
    target_link_libraries(demo_level_tests PRIVATE common)
    add_test(NAME demo_level COMMAND demo_level_tests ${CMAKE_CURRENT_SOURCE_DIR}/assets/tiled/Demo.tmx)
    add_test(NAME collision COMMAND collision_tests ${CMAKE_CURRENT_SOURCE_DIR}/assets/tiled/Sample.tmx)
endif()

# Portable package keeps the working directory one level below assets/configs.
if(WIN32)
    install(TARGETS client server RUNTIME DESTINATION bin COMPONENT Game)
    install(DIRECTORY assets shaders DESTINATION . COMPONENT Game)
    install(FILES client.toml server.toml client.bat server.bat README.md
            DESTINATION . COMPONENT Game)
    install(FILES "${sfml_SOURCE_DIR}/license.md" DESTINATION licenses/SFML COMPONENT Game)
    install(FILES "${tmxlite_SOURCE_DIR}/LICENSE" DESTINATION licenses/tmxlite COMPONENT Game)
    install(FILES "${json_SOURCE_DIR}/LICENSE.MIT" DESTINATION licenses/json COMPONENT Game)
    install(DIRECTORY third_party/tomlplusplus/ DESTINATION licenses/tomlplusplus
            COMPONENT Game FILES_MATCHING PATTERN "LICENSE*")
endif()
