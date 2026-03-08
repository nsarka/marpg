#include "common/common.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct ClientPlayer {
    common::PlayerState state;
    sf::Vector2f renderPos{0.f, 0.f};
};

class InputManager {
public:
    void handleEvent(const sf::Event& event) {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            setKey(key->code, true);
        } else if (const auto* key = event.getIf<sf::Event::KeyReleased>()) {
            setKey(key->code, false);
        } else if (event.is<sf::Event::FocusLost>()) {
            clear();
        }
    }

    sf::Vector2f movement() const {
        sf::Vector2f dir{0.f, 0.f};

        if (up_) {
            dir.y -= 1.f;
        }
        if (down_) {
            dir.y += 1.f;
        }
        if (left_) {
            dir.x -= 1.f;
        }
        if (right_) {
            dir.x += 1.f;
        }

        if (dir.x != 0.f || dir.y != 0.f) {
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            dir /= len;
        }

        return dir;
    }

private:
    void setKey(sf::Keyboard::Key key, bool pressed) {
        if (key == sf::Keyboard::Key::W) {
            up_ = pressed;
        } else if (key == sf::Keyboard::Key::S) {
            down_ = pressed;
        } else if (key == sf::Keyboard::Key::A) {
            left_ = pressed;
        } else if (key == sf::Keyboard::Key::D) {
            right_ = pressed;
        }
    }

    void clear() {
        up_ = false;
        down_ = false;
        left_ = false;
        right_ = false;
    }

    bool up_ = false;
    bool down_ = false;
    bool left_ = false;
    bool right_ = false;
};

static sf::Texture makePlayerTexture(const sf::Color& bodyColor) {
    sf::Image img({40, 40}, sf::Color::Transparent);

    const sf::Vector2f center{20.f, 20.f};
    const float radius = 18.f;

    for (unsigned y = 0; y < 40; ++y) {
        for (unsigned x = 0; x < 40; ++x) {
            const float dx = static_cast<float>(x) - center.x;
            const float dy = static_cast<float>(y) - center.y;
            const float d2 = dx * dx + dy * dy;

            if (d2 <= radius * radius) {
                img.setPixel({x, y}, bodyColor);
            }
        }
    }

    img.setPixel({14, 15}, sf::Color::Black);
    img.setPixel({26, 15}, sf::Color::Black);

    return sf::Texture(img);
}

static sf::Texture makeCollectibleTexture() {
    sf::Image img({20, 20}, sf::Color::Transparent);

    const sf::Vector2f center{10.f, 10.f};
    const float radius = 8.f;

    for (unsigned y = 0; y < 20; ++y) {
        for (unsigned x = 0; x < 20; ++x) {
            const float dx = static_cast<float>(x) - center.x;
            const float dy = static_cast<float>(y) - center.y;
            const float d2 = dx * dx + dy * dy;

            if (d2 <= radius * radius) {
                img.setPixel({x, y}, sf::Color::Yellow);
            }
        }
    }

    return sf::Texture(img);
}

static bool sendPacket(sf::UdpSocket& socket,
                       sf::Packet& packet,
                       const sf::IpAddress& ip,
                       unsigned short port,
                       const char* context) {
    const sf::Socket::Status status = socket.send(packet, ip, port);
    if (status != sf::Socket::Status::Done) {
        std::cerr << context << " failed with socket status "
                  << static_cast<int>(status) << '\n';
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    const std::string serverText = (argc >= 2) ? argv[1] : "127.0.0.1";
    const std::string myName = (argc >= 3) ? argv[2] : "Player";
    const std::string fontPath = (argc >= 4) ? argv[3] : "../assets/fonts/arial.ttf";

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

    std::cout << "Assigned player ID: " << myId << "\n";

    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned>(common::WINDOW_WIDTH),
                       static_cast<unsigned>(common::WINDOW_HEIGHT)}),
        "MARPG: Multiplayer Action RPG",
        sf::State::Windowed
    );
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile(fontPath)) {
        std::cerr << "Failed to open font: " << fontPath << "\n";
        return 1;
    }

    InputManager input;

    std::vector<ClientPlayer> players(common::MAX_PLAYERS);
    std::vector<common::CollectibleState> collectibles;

    players[myId].state.connected = true;
    players[myId].state.name = myName;

    auto tex0 = makePlayerTexture(sf::Color(80, 220, 120));
    auto tex1 = makePlayerTexture(sf::Color(80, 180, 255));
    auto collectibleTex = makeCollectibleTexture();

    sf::Sprite playerSprites[] = {
        sf::Sprite(tex0),
        sf::Sprite(tex1)
    };
    playerSprites[0].setOrigin({common::PLAYER_RADIUS, common::PLAYER_RADIUS});
    playerSprites[1].setOrigin({common::PLAYER_RADIUS, common::PLAYER_RADIUS});

    sf::Sprite collectibleSprite(collectibleTex);
    collectibleSprite.setOrigin({10.f, 10.f});

    sf::Text nameText(font, "", 16);
    nameText.setFillColor(sf::Color::White);

    sf::Text hudText(font, "", 20);
    hudText.setFillColor(sf::Color::White);
    hudText.setPosition({10.f, 8.f});

    sf::Text centerText(font, "", 28);
    centerText.setFillColor(sf::Color::White);

    sf::Clock clock;
    int connectedCount = 1;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            input.handleEvent(*event);
        }

        const sf::Vector2f dir = input.movement();

        players[myId].state.pos += dir * common::PLAYER_SPEED * dt;
        common::clampToPlayfield(players[myId].state.pos);
        players[myId].renderPos = players[myId].state.pos;

        {
            sf::Packet statePacket;
            statePacket << std::string(common::MSG_STATE)
                        << myId
                        << players[myId].state.pos.x
                        << players[myId].state.pos.y;
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

                for (int i = 0; i < common::MAX_PLAYERS; ++i) {
                    players[i].state = newStates[static_cast<std::size_t>(i)];
                    if (i == myId && players[i].renderPos == sf::Vector2f{0.f, 0.f}) {
                        players[i].renderPos = players[i].state.pos;
                    }
                }
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
            nameText.setPosition(players[i].renderPos + sf::Vector2f{-28.f, -42.f});
            window.draw(nameText);
        }

        hudText.setString(
            "You are: " + players[myId].state.name + "\n" +
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