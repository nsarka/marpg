#include "common/common.hpp"

#include <SFML/Network.hpp>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

struct ServerPlayer {
    common::PlayerState state;
    sf::IpAddress ip;
    unsigned short port = 0;
};

static sf::Vector2f randomCollectiblePos() {
    const float x = 40.f + static_cast<float>(std::rand() % 720);
    const float y = 40.f + static_cast<float>(std::rand() % 520);
    return {x, y};
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::UdpSocket socket;
    if (socket.bind(common::SERVER_PORT) != sf::Socket::Done) {
        std::cerr << "Failed to bind server socket on port " << common::SERVER_PORT << "\n";
        return 1;
    }
    socket.setBlocking(false);

    std::vector<ServerPlayer> players(common::MAX_PLAYERS);
    std::vector<common::CollectibleState> collectibles(common::NUM_COLLECTIBLES);

    for (auto& c : collectibles) {
        c.active = true;
        c.pos = randomCollectiblePos();
    }

    std::cout << "Server listening on port " << common::SERVER_PORT << "\n";

    while (true) {
        sf::Packet packet;
        sf::IpAddress senderIp;
        unsigned short senderPort = 0;

        while (socket.receive(packet, senderIp, senderPort) == sf::Socket::Done) {
            std::string type;
            packet >> type;

            if (type == common::MSG_JOIN) {
                std::string requestedName;
                packet >> requestedName;

                int assignedId = -1;
                for (int i = 0; i < common::MAX_PLAYERS; ++i) {
                    if (!players[i].state.connected) {
                        assignedId = i;
                        players[i].state.connected = true;
                        players[i].ip = senderIp;
                        players[i].port = senderPort;
                        players[i].state.name =
                            requestedName.empty() ? ("Player" + std::to_string(i + 1)) : requestedName;
                        players[i].state.score = 0;
                        players[i].state.pos = {150.f + 400.f * static_cast<float>(i), 300.f};
                        break;
                    }
                }

                sf::Packet reply;
                reply << std::string(common::MSG_JOIN_ACK) << assignedId;
                socket.send(reply, senderIp, senderPort);

                if (assignedId >= 0) {
                    std::cout << "Join from " << senderIp.toString() << ":" << senderPort
                              << " -> player " << assignedId
                              << " name=" << players[assignedId].state.name << "\n";
                } else {
                    std::cout << "Rejected join from " << senderIp.toString() << ":" << senderPort
                              << " (server full)\n";
                }
            } else if (type == common::MSG_STATE) {
                int id = -1;
                float x = 0.f;
                float y = 0.f;
                packet >> id >> x >> y;

                if (id >= 0 && id < common::MAX_PLAYERS && players[id].state.connected) {
                    players[id].ip = senderIp;
                    players[id].port = senderPort;
                    players[id].state.pos = {x, y};
                    common::clampToPlayfield(players[id].state.pos);
                }
            }
        }

        for (auto& player : players) {
            if (!player.state.connected) {
                continue;
            }

            for (auto& collectible : collectibles) {
                if (!collectible.active) {
                    continue;
                }

                const float pickupDist = common::PLAYER_RADIUS + common::PICKUP_RADIUS;
                if (common::distanceSq(player.state.pos, collectible.pos) <= pickupDist * pickupDist) {
                    player.state.score += 1;
                    collectible.active = true;
                    collectible.pos = randomCollectiblePos();
                }
            }
        }

        int connectedCount = 0;
        for (const auto& player : players) {
            if (player.state.connected) {
                ++connectedCount;
            }
        }

        std::vector<common::PlayerState> publicStates;
        publicStates.reserve(players.size());
        for (const auto& player : players) {
            publicStates.push_back(player.state);
        }

        sf::Packet worldPacket;
        common::writeWorldPacket(worldPacket, connectedCount, publicStates, collectibles);

        for (const auto& player : players) {
            if (!player.state.connected) {
                continue;
            }
            socket.send(worldPacket, player.ip, player.port);
        }

        sf::sleep(sf::milliseconds(16));
    }

    return 0;
}