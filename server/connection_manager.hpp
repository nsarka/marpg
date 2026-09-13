#pragma once
#include "build_version.hpp"
#include "common/chat.hpp"
#include "common/runtime_settings.hpp"
#include "common/slash_command.hpp"
#include "common/world_transport.hpp"
#include "game_simulation.hpp"
#include <SFML/Network.hpp>
#include <csignal>
#include <iostream>
inline bool sendPacket(sf::UdpSocket& socket, sf::Packet& packet, const sf::IpAddress& ip,
                       unsigned short port, const char* context) {
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
        std::uint32_t runtimeAck = 0, loadedWorld = 1;
        sf::Clock runtimeRetry;
        common::ChatInbox chatInbox;
        common::ChatOutbox chatOutbox;
    };
    GameSimulation* simulation;
    common::RuntimeSettings runtime;
    common::Logger& logger;
    sf::UdpSocket socket;
    std::vector<Peer> peers{common::MAX_PLAYERS};
    std::uint32_t snapshotSequence = 0, chatSequence = 0;
    sf::Clock chatClock;
    void deliverChat(const common::ChatMessage& message, std::optional<common::PlayerId> recipient = {}) {
        if (recipient)
            logger.log_info("[Chat to ", simulation->players[*recipient].state.name, "] ",
                            common::chatLine(message));
        else
            logger.log_info("[Chat] ", common::chatLine(message));
        logger.flush();
        const auto now = chatClock.getElapsedTime().asMilliseconds();
        for (unsigned i = 0; i < peers.size(); ++i)
            if ((!recipient || *recipient == i) && simulation->players[i].state.connected && peers[i].ip)
                peers[i].chatOutbox.enqueue(message.sequence, common::chatPacket(message), now);
    }
    void disconnect(common::PlayerId id) {
        auto& players = simulation->players;
        const auto name = players[id].state.name;
        simulation->remove(id);
        sendServerMessage(name + " left the game.", common::ChatKind::System);
        peers[id].lastInputSequence.reset();
        peers[id].chatInbox = {};
        peers[id].chatOutbox = {};
        // Retain the endpoint until slot reuse so repeated leave requests still get ACKed.
    }

  public:
    ConnectionManager(GameSimulation& game, common::Logger& log) : simulation(&game), logger(log) {
        const auto& settings = game.settings;
        const auto ip = sf::IpAddress::resolve(settings.ip);
        if (!ip)
            throw std::runtime_error("Invalid bind IP: " + settings.ip);
        if (socket.bind(settings.port, *ip) != sf::Socket::Status::Done)
            throw std::runtime_error("Failed to bind server socket on port " + std::to_string(settings.port));
        socket.setBlocking(false);
        logger.log_info("Server listening on port ", settings.port);
        std::cout.flush();
    }
    void pump(const volatile std::sig_atomic_t& stopRequested) {
        const auto& settings = simulation->settings;
        auto& players = simulation->players;
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
                        if (!simulation->join(i, requestedName, requestedTeam)) {
                            assignedId = -1;
                            break;
                        }
                        peers[i] = Peer{};
                        peers[i].loadedWorld = runtime.world;
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
                runtime.botMask = simulation->botMask();
                common::writeRuntime(reply, runtime);

                // Only the requester receives its assignment, including full-server rejection.
                sendPacket(socket, reply, *senderIp, senderPort, "join_ack send");
                if (!repeatedJoin && assignedId >= 0) {
                    sendServerMessage(players[assignedId].state.name + " joined the game.",
                                      common::ChatKind::System);
                    sf::Packet joined;
                    joined << std::string("player_joined") << static_cast<common::PlayerId>(assignedId);
                    for (std::size_t i = 0; i < players.size(); ++i) {
                        const auto& player = players[i];
                        const auto& peer = peers[i];
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
            } else if (type == common::MSG_CHAT_SEND || type == common::MSG_CHAT_ACK) {
                common::PlayerId id;
                std::uint32_t sequence;
                if (!(packet >> id >> sequence) || id >= players.size())
                    continue;
                auto& peer = peers[id];
                const auto& player = players[id].state;
                if (!player.connected || peer.ip != senderIp || peer.port != senderPort)
                    continue;
                peer.lastHeard.restart();
                if (type == common::MSG_CHAT_ACK) {
                    peer.chatOutbox.acknowledge(sequence);
                    continue;
                }
                std::string text;
                if (!(packet >> text) || text.size() > common::ChatMaxBytes)
                    continue;
                sf::Packet ack;
                ack << std::string(common::MSG_CHAT_SEND_ACK) << sequence;
                sendPacket(socket, ack, *senderIp, senderPort, "chat request ack");
                if (!peer.chatInbox.accept(sequence))
                    continue;
                if (const auto command = common::slashCommand(text)) {
                    try {
                        if (command->name == "team") {
                            const auto team = command->argument == "auto"
                                                  ? std::optional<unsigned>(0)
                                                  : common::commandNumber(command->argument);
                            if (!team)
                                throw std::runtime_error("Usage: /team <number|auto>");
                            sendServerMessage(simulation->changeTeam(id, *team), common::ChatKind::System,
                                              id);
                        } else if (command->name == "name") {
                            const auto name = common::cleanChatText(command->argument);
                            if (name.empty() || name.size() > 64 || name != command->argument)
                                throw std::runtime_error("Usage: /name <name> (1-64 UTF-8 bytes)");
                            const auto previous = player.name;
                            players[id].state.name = name;
                            sendServerMessage(previous + " is now known as " + name + ".");
                        } else
                            throw std::runtime_error("Unknown client command. Use /help.");
                    } catch (const std::exception& error) {
                        sendServerMessage(error.what(), common::ChatKind::System, id);
                    }
                    continue;
                }
                auto message = common::playerChat(++chatSequence, id, player, text);
                if (message.text.empty())
                    continue;
                deliverChat(message);
            } else if (type == common::MSG_RUNTIME_ACK) {
                common::PlayerId id;
                std::uint32_t revision;
                if (!(packet >> id >> revision) || id >= players.size())
                    continue;
                auto& peer = peers[id];
                if (!players[id].state.connected || peer.ip != senderIp || peer.port != senderPort ||
                    revision != runtime.revision)
                    continue;
                peer.runtimeAck = revision;
                peer.loadedWorld = runtime.world;
                peer.lastHeard.restart();
            } else if (type == "pong") {
                common::PlayerId id;
                std::uint32_t sequence;
                if (!(packet >> id >> sequence) || id >= players.size())
                    continue;
                auto& player = players[id];
                auto& peer = peers[id];
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
                auto& player = players[id];
                auto& peer = peers[id];
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
                auto& player = players[playerId];
                auto& peer = peers[playerId];
                if (!player.state.connected || peer.ip != senderIp || peer.port != senderPort)
                    continue;
                peer.lastHeard.restart();
                // ACK duplicates and expired/dead-player requests as well, so a lost
                // ACK never creates a second attack or an endless client retry.
                sf::Packet ack;
                ack << std::string(common::MSG_ATTACK_ACK) << request.sequence;
                sendPacket(socket, ack, *senderIp, senderPort, "attack ack");
                if (peer.loadedWorld == runtime.world)
                    simulation->requestAttack(playerId, request);
            } else if (type == common::MSG_STATE) {
                common::InputCommand cmd;
                common::PlayerId playerId;
                if (!readInputCmd(packet, playerId, cmd) || playerId >= players.size()) {
                    continue;
                }
                auto& player = players[playerId];
                auto& peer = peers[playerId];
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
                if (peer.loadedWorld == runtime.world)
                    simulation->input(playerId, cmd);
            }

            packet.clear();
            senderIp.reset();
            senderPort = 0;
        }

        for (common::PlayerId id = 0; id < players.size(); ++id) {
            auto& player = players[id];
            auto& peer = peers[id];
            if (player.state.connected && peer.ip && peer.lastHeard.getElapsedTime() >= sf::seconds(5))
                disconnect(id);
            if (player.state.connected && peer.ip) {
                const auto now = chatClock.getElapsedTime().asMilliseconds();
                peer.chatOutbox.expire(now);
                for (auto& message : peer.chatOutbox.due(now))
                    sendPacket(socket, message, *peer.ip, peer.port, "chat broadcast");
            }
        }
    }
    void broadcast() {
        auto& players = simulation->players;
        auto publicStates = simulation->snapshot();
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& player = players[i];
            auto& peer = peers[i];
            if (player.state.connected && peer.ip && peer.pingClock.getElapsedTime() >= sf::seconds(1)) {
                sf::Packet ping;
                ping << std::string("ping") << static_cast<common::PlayerId>(i) << ++peer.pingSequence;
                sendPacket(socket, ping, *peer.ip, peer.port, "ping");
                if (peer.pingPending)
                    player.state.pingMs = -1;
                peer.pingPending = true;
                peer.pingClock.restart();
            }
            if (player.state.connected && peer.ip && peer.runtimeAck != runtime.revision &&
                peer.runtimeRetry.getElapsedTime() >= sf::milliseconds(250)) {
                sf::Packet update;
                update << std::string(common::MSG_RUNTIME_SETTINGS);
                common::writeRuntime(update, runtime);
                common::writeSettings(update, simulation->settings);
                sendPacket(socket, update, *peer.ip, peer.port, "runtime settings");
                peer.runtimeRetry.restart();
            }
            publicStates[i].pingMs = peer.ip ? player.state.pingMs : 0;
        }

        auto packets =
            common::worldPackets(publicStates, ++snapshotSequence, simulation->killHistory.events());
        for (std::size_t i = 0; i < players.size(); ++i) {
            const auto& player = players[i];
            const auto& peer = peers[i];
            if (!player.state.connected || !peer.ip)
                continue;
            for (auto& part : packets)
                sendPacket(socket, part, *peer.ip, peer.port, "world part send");
        }
    }
    void refreshSettings() {
        ++runtime.revision;
        runtime.botMask = simulation->botMask();
        for (unsigned i = 0; i < peers.size(); ++i)
            if (simulation->players[i].state.connected && !simulation->players[i].human)
                peers[i] = Peer{};
    }
    void replaceSimulation(GameSimulation& game) {
        simulation = &game;
        ++runtime.world;
        refreshSettings();
    }
    void sendServerMessage(const std::string& text, common::ChatKind kind = common::ChatKind::Server,
                           std::optional<common::PlayerId> recipient = {}) {
        common::ChatMessage message;
        message.sequence = ++chatSequence;
        message.name = "Server";
        message.kind = kind;
        message.text = common::cleanChatText(text);
        if (message.text.empty())
            return;
        deliverChat(message, recipient);
    }
    void shutdown() {
        auto& players = simulation->players;
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
