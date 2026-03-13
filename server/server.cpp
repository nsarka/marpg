#include "common/common.hpp"
#include "common/logger.hpp"

#include <SFML/Network.hpp>

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct ServerPlayer {
    common::PlayerState state;
    std::optional<sf::IpAddress> ip;
    unsigned short port = 0;
};

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

int main() {
    common::Logger logger;

    logger.info() << "Server started";

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::UdpSocket socket;
    if (socket.bind(common::SERVER_PORT) != sf::Socket::Status::Done) {
        std::cerr << "Failed to bind server socket on port " << common::SERVER_PORT << "\n";
        return 1;
    }
    socket.setBlocking(false);

    std::vector<ServerPlayer> players(common::MAX_PLAYERS);

    std::cout << "Server listening on port " << common::SERVER_PORT << "\n";

    while (true) {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        while (socket.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done) {
            if (!senderIp) {
                packet.clear();
                continue;
            }

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
                        players[i].ip = *senderIp;
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
                sendPacket(socket, reply, *senderIp, senderPort, "join_ack send");

                if (assignedId >= 0) {
                    std::cout << "Join from " << senderIp->toString() << ":" << senderPort
                              << " -> player " << assignedId
                              << " name=" << players[assignedId].state.name << "\n";
                } else {
                    std::cout << "Rejected join from " << senderIp->toString() << ":" << senderPort
                              << " (server full)\n";
                }
            } else if (type == common::MSG_STATE) {
                int id = -1;
                float x = 0.f;
                float y = 0.f;
                packet >> id >> x >> y;

                if (id >= 0 && id < common::MAX_PLAYERS && players[id].state.connected) {
                    players[id].ip = *senderIp;
                    players[id].port = senderPort;
                    players[id].state.pos = {x, y};
                }
            }

            packet.clear();
            senderIp.reset();
            senderPort = 0;
        }

        for (auto& player : players) {
            if (!player.state.connected) {
                continue;
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

        for (const auto& player : players) {
            if (!player.state.connected || !player.ip) {
                continue;
            }

            sf::Packet worldPacket;
            common::writeWorldPacket(worldPacket, connectedCount, publicStates);
            sendPacket(socket, worldPacket, *player.ip, player.port, "world send");
        }

        sf::sleep(sf::milliseconds(16));
    }

    return 0;
}