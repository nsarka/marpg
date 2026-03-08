#include "common/common.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

struct ClientPlayer {
    common::PlayerState state;
    sf::Vector2f renderPos{0.f, 0.f};
};

static sf::Texture makePlayerTexture(const sf::Color& bodyColor) {
    sf::Image img;
    img.create(40, 40, sf::Color::Transparent);

    const sf::Vector2f center(20.f, 20.f);
    const float radius = 18.f;

    for (unsigned y = 0; y < 40; ++y) {
        for (unsigned x = 0; x < 40; ++x) {
            const float dx = static_cast<float>(x) - center.x;
            const float dy = static_cast<float>(y) - center.y;
            const float d2 = dx * dx + dy * dy;

            if (d2 <= radius * radius) {
                img.setPixel(x, y, bodyColor);
            }
        }
    }

    img.setPixel(14, 15, sf::Color::Black);
    img.setPixel(26, 15, sf::Color::Black);

    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

static sf::Texture makeCollectibleTexture() {
    sf::Image img;
    img.create(20, 20, sf::Color::Transparent);

    const sf::Vector2f center(10.f, 10.f);
    const float radius = 8.f;

    for (unsigned y = 0; y < 20; ++y) {
        for (unsigned x = 0; x < 20; ++x) {
            const float dx = static_cast<float>(x) - center.x;
            const float dy = static_cast<float>(y) - center.y;
            const float d2 = dx * dx + dy * dy;

            if (d2 <= radius * radius) {
                img.setPixel(x, y, sf::Color::Yellow);
            }
        }
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

int main(int argc, char** argv) {
    const sf::IpAddress serverIp = (argc >= 2) ? sf::IpAddress(argv[1]) : sf::IpAddress("127.0.0.1");
    const std::string myName = (argc >= 3) ? argv[2] : "Player";
    const std::string fontPath = (argc >= 4) ? argv[3] : "assets/fonts/arial.ttf";

    sf::UdpSocket socket;
    if (socket.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
        std::cerr << "Failed to bind client socket\n";
        return 1;
    }
    socket.setBlocking(false);

    {
        sf::Packet join;
        join << std::string(common::MSG_JOIN) << myName;
        if (socket.send(join, serverIp, common::SERVER_PORT) != sf::Socket::Done) {
            std::cerr << "Failed to send join packet\n";
            return 1;
        }
    }

    int myId = -2;
    std::cout << "Waiting for server...\n";

    while (myId == -2) {
        sf::Packet packet;
        sf::IpAddress senderIp;
        unsigned short senderPort = 0;

        if (socket.receive(packet, senderIp, senderPort) == sf::Socket::Done) {
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

    std::cout << "Assigned player ID: " << myId << "\n";

    sf::RenderWindow window(
        sf::VideoMode(static_cast<unsigned>(common::WINDOW_WIDTH),
                      static_cast<unsigned>(common::WINDOW_HEIGHT)),
        "Tiny SFML Multiplayer");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile(fontPath)) {
        std::cerr << "Failed to load font: " << fontPath << "\n";
        std::cerr << "Pass a font path as the 3rd argument, e.g. ./client 127.0.0.1 Alice /path/to/font.ttf\n";
        return 1;
    }

    std::vector<ClientPlayer> players(common::MAX_PLAYERS);
    std::vector<common::CollectibleState> collectibles;

    auto tex0 = makePlayerTexture(sf::Color(80, 220, 120));
    auto tex1 = makePlayerTexture(sf::Color(80, 180, 255));
    auto collectibleTex = makeCollectibleTexture();

    sf::Sprite playerSprites[2];
    playerSprites[0].setTexture(tex0);
    playerSprites[1].setTexture(tex1);
    playerSprites[0].setOrigin(common::PLAYER_RADIUS, common::PLAYER_RADIUS);
    playerSprites[1].setOrigin(common::PLAYER_RADIUS, common::PLAYER_RADIUS);

    sf::Sprite collectibleSprite;
    collectibleSprite.setTexture(collectibleTex);
    collectibleSprite.setOrigin(10.f, 10.f);

    sf::Text nameText;
    nameText.setFont(font);
    nameText.setCharacterSize(16);
    nameText.setFillColor(sf::Color::White);

    sf::Text hudText;
    hudText.setFont(font);
    hudText.setCharacterSize(20);
    hudText.setFillColor(sf::Color::White);
    hudText.setPosition(10.f, 8.f);

    sf::Text centerText;
    centerText.setFont(font);
    centerText.setCharacterSize(28);
    centerText.setFillColor(sf::Color::White);

    sf::Clock clock;
    int connectedCount = 1;
    bool showFullMessage = false;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        sf::Vector2f dir(0.f, 0.f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) dir.y -= 1.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) dir.y += 1.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) dir.x -= 1.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) dir.x += 1.f;

        if (dir.x != 0.f || dir.y != 0.f) {
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            dir /= len;
        }

        players[myId].state.pos += dir * common::PLAYER_SPEED * dt;
        common::clampToPlayfield(players[myId].state.pos);
        players[myId].renderPos = players[myId].state.pos;

        {
            sf::Packet statePacket;
            statePacket << std::string(common::MSG_STATE)
                        << myId
                        << players[myId].state.pos.x
                        << players[myId].state.pos.y;
            socket.send(statePacket, serverIp, common::SERVER_PORT);
        }

        while (true) {
            sf::Packet packet;
            sf::IpAddress senderIp;
            unsigned short senderPort = 0;

            if (socket.receive(packet, senderIp, senderPort) != sf::Socket::Done) {
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

                for (int i = 0; i < common::MAX_PLAYERS; ++i) {
                    players[i].state = newStates[static_cast<std::size_t>(i)];

                    if (i == myId && players[i].renderPos == sf::Vector2f(0.f, 0.f)) {
                        players[i].renderPos = players[i].state.pos;
                    }
                }
            } else if (type == common::MSG_FULL) {
                showFullMessage = true;
            }
        }

        for (int i = 0; i < common::MAX_PLAYERS; ++i) {
            if (!players[i].state.connected || i == myId) {
                continue;
            }

            const float t = std::clamp(common::INTERP_SPEED * dt, 0.f, 1.f);
            players[i].renderPos = common::lerp(players[i].renderPos, players[i].state.pos, t);
        }

        window.clear(sf::Color(30, 30, 30));

        for (const auto& collectible : collectibles) {
            if (!collectible.active) {
                continue;
            }
            collectibleSprite.setPosition(collectible.pos);
            window.draw(collectibleSprite);
        }

        for (int i = 0; i < common::MAX_PLAYERS; ++i) {
            if (!players[i].state.connected) {
                continue;
            }

            playerSprites[i].setPosition(players[i].renderPos);
            window.draw(playerSprites[i]);

            nameText.setString(players[i].state.name + " (" + std::to_string(players[i].state.score) + ")");
            nameText.setPosition(players[i].renderPos.x - 28.f, players[i].renderPos.y - 42.f);
            window.draw(nameText);
        }

        hudText.setString(
            "You are: " + players[myId].state.name + "\n" +
            "Players connected: " + std::to_string(connectedCount) + "/2\n" +
            "Move: WASD");
        window.draw(hudText);

        if (connectedCount < 2) {
            centerText.setString("Waiting for another player...");
            const auto bounds = centerText.getLocalBounds();
            centerText.setPosition(common::WINDOW_WIDTH / 2.f - bounds.width / 2.f, 280.f);
            window.draw(centerText);
        }

        if (showFullMessage) {
            centerText.setString("Server is full");
            const auto bounds = centerText.getLocalBounds();
            centerText.setPosition(common::WINDOW_WIDTH / 2.f - bounds.width / 2.f, 320.f);
            window.draw(centerText);
        }

        window.display();
    }

    return 0;
}