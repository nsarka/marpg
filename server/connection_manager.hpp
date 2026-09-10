#pragma once
#include "build_version.hpp"
#include "common/world_transport.hpp"
#include "game_simulation.hpp"
#include <SFML/Network.hpp>
#include <csignal>
#include <iostream>
inline bool sendPacket(sf::UdpSocket &socket, sf::Packet &packet, const sf::IpAddress &ip,
                       unsigned short port, const char *context) {
    const sf::Socket::Status status = socket.send(packet, ip, port);
    if (status != sf::Socket::Status::Done) {
        std::cerr << context << " failed with socket status " << static_cast<int>(status) << '\n';
        return false;
    }
    return true;
}

class ConnectionManager {
    struct Peer {
        std::optional<sf::IpAddress> ip;
        unsigned short port = 0;
        std::optional<std::uint32_t> lastInputSequence;
        sf::Clock lastHeard, pingClock;
        std::uint32_t pingSequence = 0;
        bool pingPending = false;
    };
    GameSimulation &simulation;
    const common::ServerSettings &settings;
    std::vector<GamePlayer> &players;
    common::Logger &logger;
    sf::UdpSocket socket;
    std::vector<Peer> peers{common::MAX_PLAYERS};
    std::uint32_t snapshotSequence = 0;
    void disconnect(common::PlayerId id) {
        logger.log_info("Player disconnected: ", id, " (", players[id].state.name, ")");
        simulation.remove(id);
        peers[id].lastInputSequence.reset();
    }

  public:
    ConnectionManager(GameSimulation &game, common::Logger &log)
        : simulation(game), settings(game.settings), players(game.players), logger(log) {
        const auto ip = sf::IpAddress::resolve(settings.ip);
        if (!ip)
            throw std::runtime_error("Invalid bind IP: " + settings.ip);
        if (socket.bind(settings.port, *ip) != sf::Socket::Status::Done)
            throw std::runtime_error("Failed to bind server socket on port " + std::to_string(settings.port));
        socket.setBlocking(false);
        logger.log_info("Server listening on port ", settings.port);
        std::cout.flush();
    }
    void pump(const volatile std::sig_atomic_t &stopRequested) {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        while (!stopRequested && socket.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done) {
            if (!senderIp) {
                packet.clear();
                continue;
            }

            std::string type;
            packet >> type;

            if (type == common::MSG_JOIN) {
                std::string requestedName;
                if (!(packet >> requestedName))
                    continue;
                std::uint32_t requestedTeam = 0;
                if (!packet.endOfPacket() && !(packet >> requestedTeam))
                    continue;
                std::string clientCommit;
                if (!packet.endOfPacket() && !(packet >> clientCommit))
                    continue;
                if (clientCommit != common::BuildCommit) {
                    sf::Packet mismatch;
                    mismatch << std::string("version_mismatch") << std::string(common::BuildCommit);
                    sendPacket(socket, mismatch, *senderIp, senderPort, "version mismatch");
                    continue;
                }

                if (requestedName.size() > 64) {
                    sf::Packet rejected;
                    rejected << std::string(common::MSG_JOIN_ACK) << std::int32_t(-1);
                    sendPacket(socket, rejected, *senderIp, senderPort, "invalid name reply");
                    continue;
                }
                int assignedId = -1;
                for (int i = 0; i < common::MAX_PLAYERS; ++i) {
                    if (players[i].state.connected && peers[i].ip == senderIp &&
                        peers[i].port == senderPort) {
                        assignedId = i;
                        break;
                    }
                }
                const bool repeatedJoin = assignedId >= 0;
                for (int i = 0; assignedId < 0 && i < static_cast<int>(settings.slots); ++i) {
                    if (!players[i].state.connected) {
                        assignedId = i;
                        logger.log_info("Assigned player id ", assignedId);
                        if (!simulation.join(i, requestedName, requestedTeam)) {
                            assignedId = -1;
                            break;
                        }
                        peers[i].ip = *senderIp;
                        peers[i].port = senderPort;
                        break;
                    }
                }

                if (assignedId >= 0)
                    peers[assignedId].lastHeard.restart();
                sf::Packet reply;
                reply << std::string(common::MSG_JOIN_ACK) << assignedId << common::ProtocolVersion
                      << std::string(common::BuildCommit);
                common::writeSettings(reply, settings);

                // Only the requester receives its assignment, including full-server rejection.
                sendPacket(socket, reply, *senderIp, senderPort, "join_ack send");
                if (!repeatedJoin && assignedId >= 0) {
                    sf::Packet joined;
                    joined << std::string("player_joined") << static_cast<common::PlayerId>(assignedId);
                    for (std::size_t i = 0; i < players.size(); ++i) {
                        const auto &player = players[i];
                        const auto &peer = peers[i];
                        if (!player.state.connected || !peer.ip)
                            continue;
                        sendPacket(socket, joined, *peer.ip, peer.port, "player_joined send");
                    }
                }

                if (assignedId >= 0) {
                    logger.info() << "Join from " << senderIp->toString() << ":" << senderPort
                                  << " -> player " << assignedId << " name=" << players[assignedId].state.name
                                  << "\n";
                } else {
                    logger.info() << "Rejected join from " << senderIp->toString() << ":" << senderPort
                                  << " (server full)\n";
                }
            } else if (type == "pong") {
                common::PlayerId id;
                std::uint32_t sequence;
                if (!(packet >> id >> sequence) || id >= players.size())
                    continue;
                auto &player = players[id];
                auto &peer = peers[id];
                if (!player.state.connected || peer.ip != senderIp || peer.port != senderPort ||
                    !peer.pingPending || sequence != peer.pingSequence)
                    continue;
                player.state.pingMs = peer.pingClock.getElapsedTime().asMilliseconds();
                peer.pingPending = false;
                peer.lastHeard.restart(); // Validated pong also keeps asset-loading clients alive.
            } else if (type == "leave") {
                common::PlayerId id;
                if (!(packet >> id) || id >= players.size())
                    continue;
                auto &player = players[id];
                auto &peer = peers[id];
                if (peer.ip != senderIp || peer.port != senderPort)
                    continue;
                if (player.state.connected)
                    disconnect(id);
                sf::Packet ack;
                ack << std::string("leave_ack") << id;
                sendPacket(socket, ack, *senderIp, senderPort, "leave ack");
            } else if (type == common::MSG_ATTACK) {
                common::PlayerId playerId;
                common::AttackRequest request;
                if (!common::readAttackRequest(packet, playerId, request) || playerId >= players.size())
                    continue;
                auto &player = players[playerId];
                auto &peer = peers[playerId];
                if (!player.state.connected || peer.ip != senderIp || peer.port != senderPort)
                    continue;
                peer.lastHeard.restart();
                // ACK duplicates and expired/dead-player requests as well, so a lost
                // ACK never creates a second attack or an endless client retry.
                sf::Packet ack;
                ack << std::string(common::MSG_ATTACK_ACK) << request.sequence;
                sendPacket(socket, ack, *senderIp, senderPort, "attack ack");
                simulation.requestAttack(playerId, request);
            } else if (type == common::MSG_STATE) {
                common::InputCommand cmd;
                common::PlayerId playerId;
                if (!readInputCmd(packet, playerId, cmd) || playerId >= players.size()) {
                    continue;
                }
                auto &player = players[playerId];
                auto &peer = peers[playerId];
                if (!player.state.connected || peer.ip != senderIp || peer.port != senderPort) {
                    continue;
                }
                peer.lastHeard.restart();
                // Ignore duplicate or out-of-order UDP inputs (including wraparound).
                if (peer.lastInputSequence && (cmd.sequence - *peer.lastInputSequence == 0 ||
                                               cmd.sequence - *peer.lastInputSequence >= 0x80000000u)) {
                    continue;
                }
                peer.lastInputSequence = cmd.sequence;
                simulation.input(playerId, cmd);
            }

            packet.clear();
            senderIp.reset();
            senderPort = 0;
        }

        for (common::PlayerId id = 0; id < players.size(); ++id) {
            auto &player = players[id];
            auto &peer = peers[id];
            if (player.state.connected && peer.ip && peer.lastHeard.getElapsedTime() >= sf::seconds(5))
                disconnect(id);
        }
    }
    void broadcast() {
        auto publicStates = simulation.snapshot();
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto &player = players[i];
            auto &peer = peers[i];
            if (player.state.connected && peer.ip && peer.pingClock.getElapsedTime() >= sf::seconds(1)) {
                sf::Packet ping;
                ping << std::string("ping") << static_cast<common::PlayerId>(i) << ++peer.pingSequence;
                sendPacket(socket, ping, *peer.ip, peer.port, "ping");
                if (peer.pingPending)
                    player.state.pingMs = -1;
                peer.pingPending = true;
                peer.pingClock.restart();
            }
            publicStates[i].pingMs = peer.ip ? player.state.pingMs : 0;
        }

        auto packets =
            common::worldPackets(publicStates, ++snapshotSequence, simulation.killHistory.events());
        for (std::size_t i = 0; i < players.size(); ++i) {
            const auto &player = players[i];
            const auto &peer = peers[i];
            if (!player.state.connected || !peer.ip)
                continue;
            for (auto &part : packets)
                sendPacket(socket, part, *peer.ip, peer.port, "world part send");
        }
    }
    void shutdown() {
        logger.log_info("Server is shutting down. Notifying clients...");
        std::vector<bool> pending(players.size());
        for (std::size_t i = 0; i < players.size(); ++i)
            pending[i] = players[i].state.connected && peers[i].ip.has_value();
        sf::Clock shutdownClock, retry;
        bool first = true;
        while (shutdownClock.getElapsedTime() < sf::milliseconds(800) &&
               std::any_of(pending.begin(), pending.end(), [](bool value) { return value; })) {
            if (first || retry.getElapsedTime() >= sf::milliseconds(100)) {
                sf::Packet notice;
                notice << std::string("server_shutdown");
                for (std::size_t i = 0; i < players.size(); ++i)
                    if (pending[i])
                        sendPacket(socket, notice, *peers[i].ip, peers[i].port, "shutdown notice");
                first = false;
                retry.restart();
            }
            sf::Packet packet;
            std::optional<sf::IpAddress> address;
            unsigned short port = 0;
            while (socket.receive(packet, address, port) == sf::Socket::Status::Done) {
                std::string kind;
                common::PlayerId id;
                if ((packet >> kind >> id) && kind == "shutdown_ack" && id < players.size() &&
                    peers[id].ip == address && peers[id].port == port)
                    pending[id] = false;
                if (shutdownClock.getElapsedTime() >= sf::milliseconds(800))
                    break;
            }
            sf::sleep(sf::milliseconds(5));
        }
    }
};
