cmake_minimum_required(VERSION 3.16)
project(TinyMultiplayerSFML)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 2.5 COMPONENTS graphics window system network REQUIRED)

add_library(common STATIC
    common/common.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(common PUBLIC
    sfml-system
    sfml-network
    sfml-graphics
)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE
    common
    sfml-network
    sfml-system
)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE
    common
    sfml-graphics
    sfml-window
    sfml-system
    sfml-network
)