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

void ClientConnection::pumpNetwork(std::vector<Player>& players) {
    while (true) {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;
        int connectedCount = 0; // nick: not really using this. it was used for the size of the players vector, but im just setting the size to MAX_PLAYERS now

        if (udp_socket_.receive(packet, senderIp, senderPort) != sf::Socket::Status::Done) {
            break;
        }

        std::string type;
        packet >> type;

        if (type == common::MSG_WORLD) {
            std::vector<common::PlayerState> newStates;
            if (!common::readWorldPacket(packet, connectedCount, newStates)) {
                continue;
            }

            if (static_cast<int>(newStates.size()) != common::MAX_PLAYERS) {
                continue;
            }

            for (int i = 0; i < common::MAX_PLAYERS; ++i) {
                //const bool wasConnected = players[i].state().connected;
                const bool isLocal = (i == myId_);

                if (isLocal) {
                    // Keep local position/render position locally controlled.
                    // Only accept metadata from the server.
                    players[i].state().connected = newStates[i].connected;
                    players[i].state().name = newStates[i].name;
                    players[i].state().score = newStates[i].score;
                } else {
                    players[i].state() = newStates[i];
                }
            }
        }
    }
}

void ClientConnection::sendInput(common::InputCommand &cmd) {
    sf::Packet packet;
    common::writeInputCmd(packet, cmd);
    if (!sendPacket(udp_socket_, packet, serverIp_, common::SERVER_PORT, "send input cmd")) {
        logger.log_error("Error sending input command to server: ", cmd);
    }
}
