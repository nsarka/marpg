#include "common/common.hpp"
#include "camera.hpp"
#include "player.hpp"
#include "client_connection.hpp"
#include "common/logger.hpp"
#include "common/map_layer.hpp"

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
    // ClientConnection conn{logger};
    common::PlayerId myId = 0;
    // if(myId = conn.connectToServer() == -1) {
    //     return 1;
    // }

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
    sf::FloatRect oldBounds = sf::FloatRect({-2000.f, -2000.f}, {4000.f, 4000.f});
    Camera camera({common::WINDOW_WIDTH, common::WINDOW_HEIGHT});
    camera.setFollowSharpness(8.f);
    camera.setDeadZone({60.f, 40.f});
    camera.setWorldBounds(oldBounds);

    // -------------------------------------------------------------------------
    // Players
    // -------------------------------------------------------------------------
    std::vector<Player> players;
    players.reserve(3);

    {
        common::PlayerState s;
        s.connected = true;
        s.alive = true;
        s.pos = {100.f, 100.f};
        s.vel = {0.f, 0.f};
        s.name = "Local";
        s.score = 0;
        players.emplace_back(s);
    }

    {
        common::PlayerState s;
        s.connected = true;
        s.alive = true;
        s.pos = {300.f, 150.f};
        s.vel = {0.f, 0.f};
        s.name = "RemoteA";
        s.score = 0;
        players.emplace_back(s);
    }

    {
        common::PlayerState s;
        s.connected = true;
        s.alive = true;
        s.pos = {-250.f, 220.f};
        s.vel = {0.f, 0.f};
        s.name = "RemoteB";
        s.score = 0;
        players.emplace_back(s);
    }

    const std::size_t localPlayerIndex = 0;

    for (int i = 0; i < 3; i++) {
        Player& p = players[i];

        p.setCharacterAnimations(resources.getCharacterAnimations("businessman"));
        p.setFont(resources.getFont("ui"));
        p.setSpriteScale({1.10f, 1.10f});
        p.setOriginToFeet();
        p.setInterpolationSharpness(14.f);
        p.setWalkSpeed(80.f);
        p.setRunSpeed(180.f);
        p.teleportTo(p.state().pos);

        if (i == localPlayerIndex) {
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
    float simAccumulator = 0.f;
    float remoteSnapshotAccumulator = 0.f;
    float elapsedTime = 0.f;

    // -------------------------------------------------------------------------
    // Main loop
    // -------------------------------------------------------------------------
    while (window.isOpen()) {
        // ---------------------------------------------------------------------
        // Render clock: real time between rendered frames
        // ---------------------------------------------------------------------
        const float renderDt = frameClock.restart().asSeconds();
        simAccumulator += renderDt;
        remoteSnapshotAccumulator += renderDt;
        elapsedTime += renderDt;

        // ---------------------------------------------------------------------
        // Events
        // ---------------------------------------------------------------------
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                switch (keyPressed->code) {
                    case sf::Keyboard::Key::J:
                        players[localPlayerIndex].playOneShot(Player::Anim::LeftJab);
                        break;
                    case sf::Keyboard::Key::K:
                        players[localPlayerIndex].playOneShot(Player::Anim::RightHook);
                        break;
                    case sf::Keyboard::Key::U:
                        players[localPlayerIndex].playOneShot(Player::Anim::Uppercut);
                        break;
                    case sf::Keyboard::Key::H:
                        players[localPlayerIndex].playOneShot(Player::Anim::Damaged);
                        break;
                    case sf::Keyboard::Key::X: {
                        common::PlayerState s = players[localPlayerIndex].state();
                        s.alive = false;
                        s.vel = {0.f, 0.f};
                        players[localPlayerIndex].applySnapshot(s);
                        break;
                    }
                    case sf::Keyboard::Key::R: {
                        common::PlayerState s = players[localPlayerIndex].state();
                        s.alive = true;
                        s.pos = {0.f, 0.f};
                        s.vel = {0.f, 0.f};
                        players[localPlayerIndex].teleportTo(s.pos);
                        players[localPlayerIndex].applySnapshot(s);
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        // ---------------------------------------------------------------------
        // Fixed simulation clock
        // ---------------------------------------------------------------------
        while (simAccumulator >= common::TICK_DT) {
            simAccumulator -= common::TICK_DT;

            // -----------------------------------------------------------------
            // Local player simulation
            //
            // This is gameplay-side movement using a FIXED timestep.
            // In a real game, this would be input -> movement -> collision ->
            // prediction -> send input command to server.
            // -----------------------------------------------------------------
            Player& localPlayer = players[localPlayerIndex];
            common::PlayerState localState = localPlayer.state();

            if (localState.connected && localState.alive) {
                sf::Vector2f moveInput{0.f, 0.f};

                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) moveInput.y -= 1.f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) moveInput.y += 1.f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) moveInput.x -= 1.f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) moveInput.x += 1.f;

                moveInput = normalizeOrZero(moveInput);

                const bool running =
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                const float speed = running ? 180.f : 80.f;

                localState.vel = moveInput * speed;
                localState.pos += localState.vel * common::TICK_DT;

                // In a real client:
                // - for the local player you might directly use prediction
                // - for remote players you would never simulate movement from input
                //   like this; you would wait for server snapshots
                localPlayer.applySnapshot(localState);

                if (localState.vel.x != 0.f || localState.vel.y != 0.f) {
                    localPlayer.setFacingFromVector(localState.vel);
                }

                logger.log_info("Player pos: ", localState.pos);
            }

            // -----------------------------------------------------------------
            // Fake remote snapshots arriving over the network
            //
            // In a real game, these come from the server asynchronously.
            // Here we just generate them periodically.
            // -----------------------------------------------------------------
            if (remoteSnapshotAccumulator >= kRemoteSnapshotInterval) {
                remoteSnapshotAccumulator = 0.f;

                // RemoteA circles
                {
                    common::PlayerState s = players[1].state();
                    s.connected = true;
                    s.alive = true;

                    const float radius = 260.f;
                    const float omega = 0.8f;
                    const float t = elapsedTime;

                    const sf::Vector2f newPos{
                        std::cos(t * omega) * radius + 200.f,
                        std::sin(t * omega) * radius + 100.f
                    };

                    s.vel = (newPos - s.pos) / kRemoteSnapshotInterval;
                    s.pos = newPos;

                    players[1].applySnapshot(s);
                    if (s.vel.x != 0.f || s.vel.y != 0.f) {
                        players[1].setFacingFromVector(s.vel);
                    }
                }

                // RemoteB moves in a lissajous-like path
                {
                    common::PlayerState s = players[2].state();
                    s.connected = true;
                    s.alive = true;

                    const float t = elapsedTime;
                    const sf::Vector2f newPos{
                        std::sin(t * 1.2f) * 350.f - 250.f,
                        std::cos(t * 0.7f) * 180.f + 200.f
                    };

                    s.vel = (newPos - s.pos) / kRemoteSnapshotInterval;
                    s.pos = newPos;

                    players[2].applySnapshot(s);
                    if (s.vel.x != 0.f || s.vel.y != 0.f) {
                        players[2].setFacingFromVector(s.vel);
                    }
                }
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
        for (Player& p : players) {
            p.update(renderDt);
        }

        if (Player* target = chooseCameraTarget(players, localPlayerIndex)) {
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

        for (const Player& p : players) {
            window.draw(p);
        }

        // Draw debug rectangles in world space
        window.draw(makeOutlinedRect(layerBounds, 2.f, sf::Color::Green));
        window.draw(makeOutlinedRect(oldBounds, 2.f, sf::Color::Red));
        window.draw(makeOutlinedRect(sf::FloatRect({0.f, 0.f}, {100.f, 100.f}), 2.f, sf::Color::Blue));

        // Draw debug rectangles in screen space
        window.setView(window.getDefaultView()); // back to screen-space
        window.draw(makeOutlinedRect(sf::FloatRect({common::WINDOW_WIDTH / 2, common::WINDOW_HEIGHT / 2}, {120.f, 80.f}), 2.f, sf::Color::Black));

        window.display();
    }

    return 0;
}


/*

#include "common/common.hpp"
#include "common/map_layer.hpp"
#include "input_manager.hpp"
#include "player.hpp"
#include "camera.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>


int main(int argc, char** argv) {
    const std::string serverText = (argc >= 2) ? argv[1] : "127.0.0.1";
    const std::string myName = (argc >= 3) ? argv[2] : "Rick";

    const std::string fontPath = "../assets/fonts/arial.ttf";

    const std::optional<sf::IpAddress> maybeIp = sf::IpAddress::resolve(serverText);
    if (!maybeIp) {
        std::cerr << "Could not resolve server address: " << serverText << "\n";
        return 1;
    }
    const sf::IpAddress serverIp = *maybeIp;

    sf::UdpSocket socket;
    if (socket.bind(sf::Socket::AnyPort) != sf::Socket::Status::Done) {
        std::cerr << "Failed to bind client socket\n";
        return 1;
    }
    socket.setBlocking(false);

    {
        sf::Packet join;
        join << std::string(common::MSG_JOIN) << myName;
        if (!sendPacket(socket, join, serverIp, common::SERVER_PORT, "join send")) {
            return 1;
        }
    }

    int myId = -2;
    std::cout << "Waiting for server...\n";

    while (myId == -2) {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        if (socket.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done) {
            std::string type;
            packet >> type;
            if (type == common::MSG_JOIN_ACK) {
                packet >> myId;
            }
        }

        sf::sleep(sf::milliseconds(10));
    }

    if (myId == -1) {
        std::cout << "Server is full.\n";
        return 0;
    }

    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned>(common::WINDOW_WIDTH),
                       static_cast<unsigned>(common::WINDOW_HEIGHT)}),
        "MARPG: Multiplayer Action RPG",
        sf::State::Windowed
    );
    window.setFramerateLimit(128);

    sf::Font font;
    if (!font.openFromFile(fontPath)) {
        std::cerr << "Failed to open font: " << fontPath << "\n";
        return 1;
    }

    InputManager input{window};

    std::vector<Player> players(common::MAX_PLAYERS);
    std::vector<common::CollectibleState> collectibles;

    players[myId].state().connected = true;
    players[myId].state().name = myName;

    auto collectibleTex = makeCollectibleTexture();

    sf::Sprite collectibleSprite(collectibleTex);
    collectibleSprite.setOrigin({10.f, 10.f});

    sf::Text nameText(font, "", 16);
    nameText.setFillColor(sf::Color::White);

    sf::Text hudText(font, "", 20);
    hudText.setFillColor(sf::Color::White);
    hudText.setPosition({10.f, 8.f});

    sf::Text centerText(font, "", 28);
    centerText.setFillColor(sf::Color::White);

    sf::Clock frameClock;
    float accumulator = 0.f;

    int connectedCount = 1;
    bool haveReceivedFirstWorld = false;

    tmx::Map map;
    map.load("../assets/tiled/Sample.tmx");

    MapLayer layerFloor(map, 1);
    MapLayer layerWalls(map, 2);
    //MapLayer layerTriggers(map, 9);

    layerWalls.update(sf::Time::Zero);
    layerFloor.update(sf::Time::Zero);

    Camera camera({common::WINDOW_WIDTH, common::WINDOW_HEIGHT});
    camera.setFollowSharpness(8.f);
    camera.setDeadZone({80.f, 50.f});
    camera.setWorldBounds(layerFloor.getGlobalBounds());

    while (window.isOpen()) {
        float frame_dt = frameClock.restart().asSeconds();
        accumulator += frame_dt;

        input.handleEvents();
        const sf::Vector2f dir = input.movement();

        // Only simulate locally after we have the first authoritative world snapshot.
        if (haveReceivedFirstWorld) {
            players[myId].state().pos += dir * common::PLAYER_SPEED * dt;

            sf::Packet statePacket;
            statePacket << std::string(common::MSG_STATE)
                        << myId
                        << players[myId].state().pos.x
                        << players[myId].state().pos.y;
            sendPacket(socket, statePacket, serverIp, common::SERVER_PORT, "state send");
        }

        while (true) {
            sf::Packet packet;
            std::optional<sf::IpAddress> senderIp;
            unsigned short senderPort = 0;

            if (socket.receive(packet, senderIp, senderPort) != sf::Socket::Status::Done) {
                break;
            }

            std::string type;
            packet >> type;

            if (type == common::MSG_WORLD) {
                std::vector<common::PlayerState> newStates;
                if (!common::readWorldPacket(packet, connectedCount, newStates, collectibles)) {
                    continue;
                }

                if (static_cast<int>(newStates.size()) != common::MAX_PLAYERS) {
                    continue;
                }

                haveReceivedFirstWorld = true;

                for (int i = 0; i < common::MAX_PLAYERS; ++i) {
                    const bool wasConnected = players[i].state().connected;
                    const bool isLocal = (i == myId);

                    if (isLocal) {
                        // Keep local position/render position locally controlled.
                        // Only accept metadata from the server.
                        players[i].state().connected = newStates[i].connected;
                        players[i].state().name = newStates[i].name;
                        players[i].state().score = newStates[i].score;

                        // On the first world packet, initialize the local position once.
                        if (!players[i].renderPosInitialized) {
                            players[i].state().pos = newStates[i].pos;
                            //players[i].renderPos = newStates[i].pos;
                            players[i].renderPosInitialized = true;
                        }
                    } else {
                        players[i].state = newStates[i];

                        if (!wasConnected && players[i].state().connected) {
                            players[i].renderPos = players[i].state().pos;
                            players[i].renderPosInitialized = true;
                        } else if (!players[i].renderPosInitialized) {
                            players[i].renderPos = players[i].state().pos;
                            players[i].renderPosInitialized = true;
                        } else if (!players[i].state().connected) {
                            players[i].renderPosInitialized = false;
                        }
                    }
                }
            }
        }

        // simulation step(s)
        while (accumulator >= sim_dt) {
            //simulate(sim_dt);
            accumulator -= sim_dt;
        }

        window.clear(sf::Color(30, 30, 30));
        window.setView(camera.view());

        sf::Vector2f newOffset = sf::Vector2f(-players[myId].state().pos.x, -players[myId].state().pos.y);
        layerWalls.setOffset(newOffset);
        layerFloor.setOffset(newOffset);
        window.draw(layerFloor);
        window.draw(layerWalls);
        //window.draw(layerTrigger);

        for (const auto& collectible : collectibles) {
            if (!collectible.active) {
                continue;
            }
            collectibleSprite.setPosition(collectible.pos - players[myId].state().pos + center_window);
            window.draw(collectibleSprite);
        }

        for (int i = 0; i < common::MAX_PLAYERS; ++i) {
            players[i].update(frame_dt);
            if (!players[i].state().connected) {
                continue;
            }
            if (!players[i].renderPosInitialized) {
                continue;
            }

            window.draw(player);
        }

        hudText.setString(
            "You are: " + players[myId].state().name + "\n" +
            "Players connected: " + std::to_string(connectedCount) + "/2\n" +
            "Move: WASD"
        );
        window.draw(hudText);

        if (connectedCount < 2) {
            centerText.setString("Waiting for another player...");
            const auto bounds = centerText.getLocalBounds();
            centerText.setPosition({
                common::WINDOW_WIDTH / 2.f - bounds.size.x / 2.f,
                280.f
            });
            window.draw(centerText);
        }

        window.display();
    }

    return 0;
}
*/
