#include "common/common.hpp"
#include "common/logger.hpp"

#include <SFML/Network.hpp>

#include <cstdlib>
#include <ctime>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct ServerPlayer {
    common::PlayerState state;
    std::optional<sf::IpAddress> ip;
    unsigned short port = 0;
};

namespace {

bool sendPacket(sf::UdpSocket& socket, sf::Packet& packet, const sf::IpAddress& ip, unsigned short port, const char* context) {
    const sf::Socket::Status status = socket.send(packet, ip, port);
    if (status != sf::Socket::Status::Done) {
        std::cerr << context << " failed with socket status "
                  << static_cast<int>(status) << '\n';
        return false;
    }
    return true;
}

common::PlayerId botId = 0;
void initializeBot(std::vector<ServerPlayer>& players) {
    players[botId].state.alive = true;
    players[botId].state.connected = true;
    players[botId].state.pos.x = 350.f;
    players[botId].state.pos.y = -350.f;
}

} // namespace

int main() {
    common::Logger logger;

    logger.info() << "Server started";

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::UdpSocket socket;
    if (socket.bind(common::SERVER_PORT) != sf::Socket::Status::Done) {
        logger.log_error("Failed to bind server socket on port ", common::SERVER_PORT);
        return 1;
    }
    socket.setBlocking(false);

    std::vector<ServerPlayer> players(common::MAX_PLAYERS);
    initializeBot(players);

    logger.log_info("Server listening on port ", common::SERVER_PORT);

    float elapsedTime = 0.0f;
    common::Tick tick = 0;
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
                        logger.log_info("Assigned player id ", assignedId);
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
                    logger.info() << "Join from " << senderIp->toString() << ":" << senderPort
                              << " -> player " << assignedId
                              << " name=" << players[assignedId].state.name << "\n";
                } else {
                    logger.info() << "Rejected join from " << senderIp->toString() << ":" << senderPort
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

        // Update
        // for (auto& player : players) {
        //     if (!player.state.connected) {
        //         continue;
        //     }
        // }
        // RemoteB moves in a lissajous-like path
        {
            common::PlayerState& s = players[botId].state;
            const float t = elapsedTime;
            const sf::Vector2f newPos{
                std::sin(t * 1.2f) * 350.f - 250.f,
                std::cos(t * 0.7f) * 180.f + 200.f
            };

            s.vel = (newPos - s.pos) / 10.f;
            s.pos = newPos;
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

        sf::sleep(sf::milliseconds(common::TICK_DT * 1000.f));
    }

    return 0;
}