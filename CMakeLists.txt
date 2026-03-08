cmake_minimum_required(VERSION 3.28)
project(marpg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

FetchContent_MakeAvailable(SFML tmxlite)

add_library(common STATIC
    common/common.cpp
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

add_executable(server server.cpp)
target_link_libraries(server PRIVATE
    common
    SFML::System
    SFML::Network
)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE
    common
    SFML::System
    SFML::Window
    SFML::Graphics
    SFML::Network
)