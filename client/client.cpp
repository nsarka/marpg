#include "common/common.hpp"
#include "camera.hpp"
#include "player.hpp"
#include "client_connection.hpp"
#include "common/logger.hpp"
#include "common/map_layer.hpp"
#include "input_manager.hpp"
#include "hud.hpp"

#include <SFML/Graphics.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

namespace {
constexpr float kRemoteSnapshotInterval = 0.10f; // 10 Hz fake network updates

sf::Vector2f normalizeOrZero(sf::Vector2f v) {
    const float len2 = v.x * v.x + v.y * v.y;
    if (len2 <= 0.000001f) {
        return {0.f, 0.f};
    }

    const float len = std::sqrt(len2);
    return {v.x / len, v.y / len};
}

Player* chooseCameraTarget(std::vector<Player>& players, std::size_t localIndex) {
    if (localIndex < players.size()) {
        Player& local = players[localIndex];
        if (local.isConnected() && local.isAlive()) {
            return &local;
        }
    }

    for (Player& p : players) {
        if (p.isConnected() && p.isAlive()) {
            return &p;
        }
    }

    for (Player& p : players) {
        if (p.isConnected()) {
            return &p;
        }
    }

    return nullptr;
}

sf::RectangleShape makeOutlinedRect(const sf::FloatRect& rect,
                                    float thickness = 2.f,
                                    sf::Color color = sf::Color::Red)
{
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineThickness(thickness);
    shape.setOutlineColor(color);
    return shape;
}

} // namespace

int main() {
    common::Logger logger;

    logger.info() << "Client started";

    sf::RenderWindow window(sf::VideoMode({(int)common::WINDOW_WIDTH, (int)common::WINDOW_HEIGHT}), "Networked Player + Camera");
    window.setFramerateLimit(144);

    const std::filesystem::path assetRoot = "../assets/characters/businessman";
    const std::filesystem::path fontPath  = "../assets/fonts/arial.ttf";
    const std::filesystem::path fragPath  = "../shaders/sprite_outline.frag";

    ResourceManager resources(logger);

    if (!resources.loadFont("ui", fontPath)) {
        return 1;
    }

    if (!resources.loadFragmentShader("sprite_outline", fragPath)) {
        return 1;
    }

    if (!resources.loadBusinessmanCharacter("businessman", assetRoot)) {
        return 1;
    }

    // -------------------------------------------------------------------------
    // Client connection
    // -------------------------------------------------------------------------
    ClientConnection client_conn{logger};
    common::PlayerId myId = client_conn.connectToServer();
    if(myId == -1) {
        return 1;
    }
    logger.log_info("Assigned player id ", myId);

    // -------------------------------------------------------------------------
    // Level setup
    // -------------------------------------------------------------------------
    tmx::Map map;
    map.load("../assets/tiled/Sample.tmx");
    MapLayer layerFloor(map, 1);
    MapLayer layerWalls(map, 2);
    //MapLayer layerTriggers(map, 9);
    layerWalls.update(sf::Time::Zero);
    layerFloor.update(sf::Time::Zero);
    logger.log_info("World bounds: ", layerFloor.getGlobalBounds());

    // -------------------------------------------------------------------------
    // Camera setup
    // -------------------------------------------------------------------------
    sf::FloatRect layerBounds = layerFloor.getGlobalBounds();
    Camera camera({common::WINDOW_WIDTH, common::WINDOW_HEIGHT});
    camera.setFollowSharpness(8.f);
    camera.setDeadZone({60.f, 40.f});
    camera.setWorldBounds(layerBounds);

    // -------------------------------------------------------------------------
    // Hud setup
    // -------------------------------------------------------------------------
    Hud hud(resources.getFont("ui"));
    hud.setWindowSize({common::WINDOW_WIDTH, common::WINDOW_HEIGHT});
    hud.setPlayerName("Rick");
    hud.setHealth(100.f, 100.f);
    hud.setStamina(100.f, 100.f);
    hud.setPingMs(0);

    // -------------------------------------------------------------------------
    // Input manager setup
    // -------------------------------------------------------------------------
    InputManager input{window};

    // -------------------------------------------------------------------------
    // Players: Complete Player for everybody, and an extra PlayerState for running
    // the local simulation for the client
    // -------------------------------------------------------------------------
    std::vector<Player> players{common::MAX_PLAYERS};
    //common::PlayerState simulatedLocalPlayer{.connected = true, .alive = true};

    for (int i = 0; i < players.size(); i++) {
        Player& p = players[i];

        if (!p.state().connected) {
            continue;
        }

        p.setCharacterAnimations(resources.getCharacterAnimations("businessman"));
        p.setFont(resources.getFont("ui"));
        p.setSpriteScale({1.10f, 1.10f});
        p.setOriginToFeet();
        p.setInterpolationSharpness(14.f);
        p.setWalkSpeed(80.f);
        p.setRunSpeed(180.f);
        p.teleportTo(p.state().pos);

        if (i == myId) {
            p.setOutlineEnabled(false);
            p.setOutlineShader(nullptr);
        } else {
            p.setOutlineEnabled(true);
            p.setOutlineShader(&resources.getShader("sprite_outline"));
            p.setOutlineColor(sf::Color(255, 70, 70, 220));
            p.setOutlineThickness(1.f);
        }
    }

    // -------------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------------
    sf::Clock frameClock;
    float accumulator = 0.f;

    // -------------------------------------------------------------------------
    // Main loop
    // -------------------------------------------------------------------------
    while (window.isOpen()) {
        // ---------------------------------------------------------------------
        // Render clock: real time between rendered frames
        // ---------------------------------------------------------------------
        const float renderDt = frameClock.restart().asSeconds();
        accumulator += renderDt;

        // ---------------------------------------------------------------------
        // Events
        // ---------------------------------------------------------------------
        input.handleEvents();

        // ---------------------------------------------------------------------
        // Network
        // ---------------------------------------------------------------------
        std::vector<common::PlayerState> newStates(common::MAX_PLAYERS);
        client_conn.pumpNetwork(newStates);
        //simulatedLocalPlayer = players[myId].state(); // Set the sim state to the authoratative state

        // ---------------------------------------------------------------------
        // Fixed simulation clock
        // ---------------------------------------------------------------------
        while (accumulator >= common::TICK_DT) {
            accumulator -= common::TICK_DT;

            // Build, send, then simulate the local player's input command
            common::InputCommand input_cmd = input.buildCommand();
            client_conn.sendInput(input_cmd);

            // -----------------------------------------------------------------
            // Local player simulation
            //
            // This is gameplay-side movement using a FIXED timestep.
            // In a real game, this would be input -> movement -> collision ->
            // prediction -> send input command to server.
            // -----------------------------------------------------------------
            // Player& localPlayer = players[myId];

            // {
            //     const float speed = input_cmd.sprint ? 180.f : 80.f;

            //     simulatedLocalPlayer.vel = input_cmd.move * speed;
            //     simulatedLocalPlayer.pos += simulatedLocalPlayer.vel * common::TICK_DT;

            //     localPlayer.applySnapshot(simulatedLocalPlayer);
            // }


            for (int i = 0; i < common::MAX_PLAYERS; i++) {
                auto& p = players[i];
                if (!p.state().connected) {
                    continue;
                }
                p.applySnapshot(newStates[i]);
                //logger.log_info("Player ", i, "'s state: ", p.state());
            }
        }

        // ---------------------------------------------------------------------
        // Render/update phase
        //
        // This uses RENDER DT, not fixed simulation dt.
        // It is for:
        // - visual smoothing/interpolation
        // - animation
        // - camera smoothing
        // ---------------------------------------------------------------------
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& p = players[i];
            if (!p.state().connected) {
                continue;
            }
            p.update(renderDt);
        }

        // -------------------------------------------------------------------------
        // Update hud
        // -------------------------------------------------------------------------
        hud.setHealth(players[myId].state().health, 100.f);
        hud.setStamina(100, 100);
        //hud.setPingMs(client_conn.pingMs());
        hud.setCenterMessage("Waiting for other players...");
        hud.setFps(1.f / std::max(renderDt, 0.0001f));

        // -------------------------------------------------------------------------
        // Update camera
        // -------------------------------------------------------------------------
        if (Player* target = chooseCameraTarget(players, myId)) {
            camera.follow(target->renderPosition());
        } else {
            camera.clearFollowTarget();
        }

        camera.update(renderDt);
        window.setView(camera.view());

        // ---------------------------------------------------------------------
        // Draw
        // ---------------------------------------------------------------------
        window.clear(sf::Color(30, 34, 42));

        window.draw(layerFloor);
        window.draw(layerWalls);
        //window.draw(layerTrigger);

        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& p = players[i];
            if (!p.state().connected) {
                continue;
            }
            window.draw(p);
        }

        // Draw debug rectangles in world space
        //window.draw(makeOutlinedRect(layerBounds, 2.f, sf::Color::Green)); // doesnt show up
        window.draw(makeOutlinedRect(sf::FloatRect({300.f, 300.f}, {100.f, 100.f}), 2.f, sf::Color::Blue));

        // Draw hud & debug rectangles in screen space
        window.setView(window.getDefaultView()); // back to screen-space
        hud.draw(window);
        //window.draw(makeOutlinedRect(sf::FloatRect({common::WINDOW_WIDTH / 2, common::WINDOW_HEIGHT / 2}, {120.f, 80.f}), 2.f, sf::Color::Black));

        window.display();
    }

    return 0;
}
