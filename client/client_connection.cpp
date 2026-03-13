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


ClientConnection::ClientConnection(common::Logger& logger) : logger(logger) {}

common::PlayerId ClientConnection::connectToServer(const std::string serverText, const std::string myName) {
    logger.log_info("Connecting to ", serverText, " as ", myName);

    const std::optional<sf::IpAddress> maybeIp = sf::IpAddress::resolve(serverText);
    if (!maybeIp) {
        logger.log_error("Could not resolve server address: ", serverText);
        return -1;
    }
    const sf::IpAddress serverIp = *maybeIp;

    if (udp_socket_.bind(sf::Socket::AnyPort) != sf::Socket::Status::Done) {
        std::cerr << "Failed to bind client udp_socket_\n";
        logger.log_error("Failed to bind client udp_socket_");
        return -1;
    }
    udp_socket_.setBlocking(false);

    {
        sf::Packet join;
        join << std::string(common::MSG_JOIN) << myName;
        if (!sendPacket(udp_socket_, join, serverIp, common::SERVER_PORT, "join send")) {
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

    return (common::PlayerId)myId;
}
