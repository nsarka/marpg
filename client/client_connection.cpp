#include "client_connection.hpp"

namespace {

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

} // namespace


ClientConnection::ClientConnection(common::Logger& logger) : logger(logger), serverIp_(sf::IpAddress::LocalHost) {}


common::PlayerId ClientConnection::connectToServer(const std::string serverText, const std::string myName) {
    logger.log_info("Connecting to ", serverText, " as ", myName);

    const std::optional<sf::IpAddress> maybeIp = sf::IpAddress::resolve(serverText);
    if (!maybeIp) {
        logger.log_error("Could not resolve server address: ", serverText);
        return -1;
    }
    serverIp_ = *maybeIp;

    if (udp_socket_.bind(sf::Socket::AnyPort) != sf::Socket::Status::Done) {
        std::cerr << "Failed to bind client udp_socket_\n";
        logger.log_error("Failed to bind client udp_socket_");
        return -1;
    }
    udp_socket_.setBlocking(false);

    {
        sf::Packet join;
        join << std::string(common::MSG_JOIN) << myName;
        if (!sendPacket(udp_socket_, join, serverIp_, common::SERVER_PORT, "join send")) {
            return -1;
        }
    }

    int myId = -2;
    logger.log_info("Waiting for server...");

    while (myId == -2) {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        if (udp_socket_.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done) {
            std::string type;
            packet >> type;
            if (type == common::MSG_JOIN_ACK) {
                packet >> myId;
            }
        }

        sf::sleep(sf::milliseconds(10));
    }

    if (myId == -1) {
        logger.log_error("Server is full.");
        return -1;
    }

    myId_ = myId;

    return myId_;
}

void ClientConnection::pumpNetwork(std::vector<common::PlayerState>& newStates, std::vector<common::PlayerId>& joinedPlayers) {
    while (true) {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        if (udp_socket_.receive(packet, senderIp, senderPort) != sf::Socket::Status::Done) {
            break;
        }

        if (senderIp!=serverIp_ || senderPort!=common::SERVER_PORT) continue;
        std::string type;
        packet >> type;
        if (type == common::MSG_ATTACK_ACK) {
            std::uint32_t sequence;
            if (packet >> sequence) attackOutbox_.acknowledge(sequence);
            continue;
        }

        if (type == common::MSG_WORLD) {
            if (!common::readWorldPacket(packet, newStates)) {
                logger.log_error("Error reading new world states");
                continue;
            }
            if (myId_<newStates.size() && !newStates[myId_].alive) attackOutbox_.clear();
        }

        if (type == common::MSG_JOIN_ACK) {
            common::PlayerId joined_player_id;
            packet >> joined_player_id;
            newStates[joined_player_id].connected = true;
            joinedPlayers.push_back(joined_player_id);
            logger.log_info("join ack for ", joined_player_id);
        }
    }
}

void ClientConnection::sendInput(common::PlayerId &id, common::InputCommand &cmd) {
    const auto nowMs=static_cast<std::uint32_t>(attackClock_.getElapsedTime().asMilliseconds());
    if (cmd.jabPressed || cmd.hookPressed)
        attackOutbox_.enqueue(cmd.jabPressed ? common::AttackKind::Jab : common::AttackKind::Hook,cmd.aim,nowMs);
    for (const auto& request : attackOutbox_.requests(nowMs)) {
        auto attack=common::attackPacket(id,request);
        sendPacket(udp_socket_,attack,serverIp_,common::SERVER_PORT,"attack send");
    }
    sf::Packet packet;
    common::writeInputCmd(packet, id, cmd);
    if (!sendPacket(udp_socket_, packet, serverIp_, common::SERVER_PORT, "send input cmd")) {
        logger.log_error("Error sending input command to server: ", cmd);
    }
}
