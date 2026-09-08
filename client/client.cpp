#include "common/common.hpp"
#include "common/collision_world.hpp"
#include "common/trigger_system.hpp"
#include "camera.hpp"
#include "player.hpp"
#include "client_connection.hpp"
#include "common/logger.hpp"
#include "common/map_layer.hpp"
#include "input_manager.hpp"
#include "hud.hpp"
#include "wall_occlusion.hpp"
#include "combat_debug.hpp"
#include "damage_numbers.hpp"
#include "sound_system.hpp"

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
        if (local.isConnected()) {
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

int main(int argc, char** argv) {
    const bool help=argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h");
    if (help || argc>3) {
        auto& output=help ? std::cout : std::cerr;
        output << "Usage: " << argv[0] << " [player-name] [server-ip]\n"
               << "Defaults: player-name=Rick, server-ip=127.0.0.1\n";
        return help ? 0 : 1;
    }
    const std::string playerName=argc>1 ? argv[1] : "Rick";
    const std::string serverAddress=argc>2 ? argv[2] : "127.0.0.1";
    if (playerName.empty() || serverAddress.empty()) {
        std::cerr << "Player name and server IP must not be empty.\n";
        return 1;
    }

    common::Logger logger;

    logger.info() << "Client started";

    sf::RenderWindow window(sf::VideoMode({(int)common::WINDOW_WIDTH, (int)common::WINDOW_HEIGHT}), "MARPG");
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

    if (!resources.loadFragmentShader("player_occlusion", "../shaders/player_occlusion.frag")) return 1;

    if (!resources.loadBusinessmanCharacter("businessman", assetRoot)) {
        return 1;
    }

    // -------------------------------------------------------------------------
    // Client connection
    // -------------------------------------------------------------------------
    ClientConnection client_conn{logger};
    common::PlayerId myId = client_conn.connectToServer(serverAddress, playerName);
    if(myId == -1) {
        return 1;
    }
    logger.log_info("Assigned player id ", myId);

    // -------------------------------------------------------------------------
    // Level setup
    // -------------------------------------------------------------------------
    tmx::Map map;
    if (!map.load(common::LEVEL_PATH)) {
        logger.log_error("Failed to load level: ", common::LEVEL_PATH);
        return 1;
    }
    common::CollisionWorld collision;
    collision.load(common::LEVEL_PATH);
    std::vector<sf::ConvexShape> collisionDebugShapes;
    for (const auto& points : collision.outlines()) {
        sf::ConvexShape shape(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) shape.setPoint(i, points[i]);
        shape.setFillColor(sf::Color(255, 70, 90, 55));
        shape.setOutlineColor(sf::Color(255, 80, 100));
        shape.setOutlineThickness(1.5f);
        collisionDebugShapes.push_back(std::move(shape));
    }
    common::TriggerSystem triggers;
    triggers.load(common::LEVEL_PATH);
    for (const auto& points : triggers.outlines()) {
        sf::ConvexShape shape(points.size());
        for (std::size_t i=0; i<points.size(); ++i) shape.setPoint(i, points[i]);
        shape.setFillColor(sf::Color(255,180,40,40));
        shape.setOutlineColor(sf::Color(255,180,40));
        shape.setOutlineThickness(1.5f);
        collisionDebugShapes.push_back(std::move(shape));
    }
    std::vector<sf::Text> levelLabels;
    for (const auto& mapLayer : map.getLayers()) {
        if (mapLayer->getType() != tmx::Layer::Type::Object || mapLayer->getName() != "Labels") continue;
        for (const auto& object : mapLayer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
            sf::Text label(resources.getFont("ui"), object.getName(), 18);
            const auto p = object.getPosition();
            const float scale = float(map.getTileSize().x) / (2.f * map.getTileSize().y);
            label.setPosition({(p.x - p.y) * scale, (p.x + p.y) * 0.5f});
            const auto bounds = label.getLocalBounds();
            label.setOrigin({bounds.position.x + bounds.size.x * 0.5f, 0.f});
            label.setFillColor(sf::Color(245, 235, 200));
            label.setOutlineColor(sf::Color(25, 25, 30));
            label.setOutlineThickness(2.f);
            levelLabels.push_back(std::move(label));
        }
    }
    MapLayer layerFloor(map, 1);
    MapLayer layerWalls(map, 2);
    WallOcclusion wallOcclusion(map, 2);
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
    hud.setPlayerName(playerName);
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
    bool shouldDrawWaitingForPlayers = true;
    int connectedPlayers = 0; // used for setting shouldDrawWaitingForPlayers
    std::vector<common::PlayerId> joined_players; // pushed when a player joined later than me in order to set his connected = true. popped after it gets set
    //common::PlayerState simulatedLocalPlayer{.connected = true, .alive = true};

    // Initialize world based on what the server tells us the state is the first time
    std::vector<common::PlayerState> newStates(common::MAX_PLAYERS);
    client_conn.pumpNetwork(newStates, joined_players);
    for (int i = 0; i < common::MAX_PLAYERS; i++) {
        Player& p = players[i];

        p.state() = newStates[i];

        p.setCharacterAnimations(resources.getCharacterAnimations("businessman"));
        p.setFont(resources.getFont("ui"));
        p.setSpriteScale({1.10f, 1.10f});
        p.setOriginToFeet();
        p.setOcclusionShader(&resources.getShader("player_occlusion"));
        p.setInterpolationSharpness(14.f);
        p.setWalkSpeed(common::WALK_SPEED * 0.5f);
        p.setRunSpeed((common::WALK_SPEED + common::RUN_SPEED) * 0.5f);
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
    SoundSystem sounds("../assets/sound/Retro_Combat_FX");
    sf::Clock frameClock;
    float accumulator = 0.f;
    DamageNumbers damageNumbers;
    float damageFlash = 0.f;
    constexpr float damageFlashDuration = 0.35f;

    // -------------------------------------------------------------------------
    // Main loop
    // -------------------------------------------------------------------------
    while (window.isOpen()) {
        // ---------------------------------------------------------------------
        // Render clock: real time between rendered frames
        // ---------------------------------------------------------------------
        const float renderDt = frameClock.restart().asSeconds();
        accumulator += renderDt;
        damageFlash = std::max(0.f, damageFlash-renderDt);
        damageNumbers.update(renderDt);

        // ---------------------------------------------------------------------
        // Events
        // ---------------------------------------------------------------------
        input.handleEvents();

        // ---------------------------------------------------------------------
        // Network
        // ---------------------------------------------------------------------
        client_conn.pumpNetwork(newStates, joined_players);
        damageNumbers.observe(newStates, myId);
        sounds.observe(newStates, players[myId].renderPosition());
        while(joined_players.size() > 0) {
            const common::PlayerId p = joined_players.back();
            joined_players.pop_back();
            players[p].state().connected = true;
        }
        //simulatedLocalPlayer = players[myId].state(); // Set the sim state to the authoratative state

        // ---------------------------------------------------------------------
        // Fixed simulation clock
        // ---------------------------------------------------------------------
        while (accumulator >= common::TICK_DT) {
            accumulator -= common::TICK_DT;

            // Build, send, then simulate the local player's input command
            common::InputCommand input_cmd = input.buildCommand(players[myId].renderPosition(), camera.view());
            client_conn.sendInput(myId, input_cmd);

            // -----------------------------------------------------------------
            // Local player simulation
            //
            // This is gameplay-side movement using a FIXED timestep.
            // In a real game, this would be input -> movement -> collision ->
            // prediction -> send input command to server.
            // -----------------------------------------------------------------
            // Player& localPlayer = players[myId];

            // {
            //     const float speed = input_cmd.sprint ? common::RUN_SPEED : common::WALK_SPEED;

            //     simulatedLocalPlayer.vel = input_cmd.move * speed;
            //     simulatedLocalPlayer.pos += simulatedLocalPlayer.vel * common::TICK_DT;

            //     localPlayer.applySnapshot(simulatedLocalPlayer);
            // }


            for (int i = 0; i < common::MAX_PLAYERS; i++) {
                auto& p = players[i];
                if (!p.state().connected) {
                    continue;
                }
                if (i == myId && newStates[i].connected && newStates[i].health < p.state().health)
                    damageFlash = damageFlashDuration;
                if (i == myId && !p.isAlive() && newStates[i].alive) camera.snapTo(newStates[i].pos);
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
        connectedPlayers = 0;
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& p = players[i];
            if (!p.state().connected) {
                continue;
            }
            connectedPlayers++;

            // Check if near another player
            p.setCombatIdle(false);
            for (int j = 0; j < common::MAX_PLAYERS; j++) {
                auto& p_other = players[j];
                if (!p_other.state().connected || j == i) {
                    continue;
                }
                if (common::distance(p.state().pos, p_other.state().pos) < 150.f) {
                    p.setCombatIdle(true);
                    p_other.setCombatIdle(true);    
                }
            }

            p.update(renderDt);
        }

        // Me and the bot
        shouldDrawWaitingForPlayers = (connectedPlayers <= 2);

        // -------------------------------------------------------------------------
        // Update hud
        // -------------------------------------------------------------------------
        hud.setHealth(players[myId].state().health, 100.f);
        hud.setStamina(100, 100);
        //hud.setPingMs(client_conn.pingMs());
        if (shouldDrawWaitingForPlayers) {
            hud.setCenterMessage("Waiting for other players...");
        } else {
            hud.clearCenterMessage();
        }
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
        wallOcclusion.update(window.getSize(), camera.view());
        auto& visibilityShader = resources.getShader("player_occlusion");
        visibilityShader.setUniform("wallDepth", wallOcclusion.texture());
        visibilityShader.setUniform("renderSize", sf::Glsl::Vec2(window.getSize()));
        window.clear(sf::Color(30, 34, 42));

        window.draw(layerFloor);
        window.draw(layerWalls);
        for (const auto& label : levelLabels) window.draw(label);
        //window.draw(layerTrigger);

        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& p = players[i];
            if (!p.state().connected) {
                continue;
            }
            window.draw(p);
        }

        if (input.collisionDebugEnabled()) {
            for (const auto& shape : collisionDebugShapes) window.draw(shape);
            sf::CircleShape feet(common::CollisionWorld::PlayerRadius);
            feet.setOrigin({common::CollisionWorld::PlayerRadius, common::CollisionWorld::PlayerRadius});
            feet.setFillColor(sf::Color::Transparent);
            feet.setOutlineColor(sf::Color(80, 255, 180));
            feet.setOutlineThickness(1.5f);
            for (auto& player : players) {
                if (!player.isConnected()) continue;
                feet.setPosition(player.state().pos);
                window.draw(feet);
            }
            drawCombatDebug(window, players, collision, resources.getFont("ui"));
        }

        damageNumbers.draw(window, resources.getFont("ui"));

        // Draw debug rectangles in world space
        //window.draw(makeOutlinedRect(layerBounds, 2.f, sf::Color::Green)); // doesnt show up
        //window.draw(makeOutlinedRect(sf::FloatRect({300.f, 300.f}, {100.f, 100.f}), 2.f, sf::Color::Blue));

        // Draw hud & debug rectangles in screen space
        window.setView(window.getDefaultView()); // back to screen-space
        //hud.draw(window);
        //window.draw(makeOutlinedRect(sf::FloatRect({common::WINDOW_WIDTH / 2, common::WINDOW_HEIGHT / 2}, {120.f, 80.f}), 2.f, sf::Color::Black));

        if (input.collisionDebugEnabled()) {
            sf::Text legend(resources.getFont("ui"),
                "F1 combat: amber windup | red active | green hit | gray recovery\n"
                "Target lines: cyan clear | red wall blocked | gray outside arc\n"
                "Attack zones and feet show server positions", 13);
            legend.setPosition({12,12});
            legend.setOutlineColor(sf::Color::Black);legend.setOutlineThickness(1.f);
            window.draw(legend);
        }
        if (damageFlash > 0.f) {
            sf::RectangleShape flash(sf::Vector2f(window.getSize()));
            const float fade = damageFlash / damageFlashDuration;
            flash.setFillColor(sf::Color(220, 15, 25, static_cast<std::uint8_t>(70.f*fade*fade)));
            window.draw(flash);
        }
        window.display();
    }

    return 0;
}
