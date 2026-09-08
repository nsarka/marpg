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

    int myId = -2;
    logger.log_info("Waiting for server...");
    sf::Clock timeout, retry;
    bool firstAttempt=true;
    while (myId == -2 && timeout.getElapsedTime()<sf::seconds(10)) {
        if (firstAttempt || retry.getElapsedTime()>=sf::milliseconds(500)) {
            sf::Packet join;
            join << std::string(common::MSG_JOIN) << myName;
            if (!sendPacket(udp_socket_,join,serverIp_,common::SERVER_PORT,"join send")) return -1;
            firstAttempt=false;
            retry.restart();
        }
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort=0;
        while (udp_socket_.receive(packet,senderIp,senderPort)==sf::Socket::Status::Done) {
            if (senderIp!=serverIp_ || senderPort!=common::SERVER_PORT) continue;
            std::string type;
            int assignedId;
            if ((packet >> type) && type==common::MSG_JOIN_ACK && (packet >> assignedId) &&
                assignedId>=-1 && assignedId<common::MAX_PLAYERS) {
                myId=assignedId;
                break;
            }
        }
        sf::sleep(sf::milliseconds(10));
    }
    if (myId == -2) {
        logger.log_error("No join reply from ",serverText,":",common::SERVER_PORT,
                         " after 10 seconds. Check the server, IP address, and Windows/WSL firewall or return UDP traffic.");
        return -1;
    }

    if (myId == -1) {
        logger.log_error("Server is full.");
        return -1;
    }

    lastWorld_.restart();
    myId_ = myId;

    return myId_;
}

ClientConnection::~ClientConnection() { leaveServer(); }

void ClientConnection::leaveServer() {
    if (myId_>=common::MAX_PLAYERS) return;
    sf::Clock timeout,retry;
    bool first=true,acknowledged=false;
    while (!acknowledged && timeout.getElapsedTime()<sf::milliseconds(300)) {
        if (first || retry.getElapsedTime()>=sf::milliseconds(75)) {
            sf::Packet leave;
            leave << std::string("leave") << myId_;
            sendPacket(udp_socket_,leave,serverIp_,common::SERVER_PORT,"leave send");
            first=false; retry.restart();
        }
        sf::Packet packet;
        std::optional<sf::IpAddress> sender;
        unsigned short port=0;
        while (udp_socket_.receive(packet,sender,port)==sf::Socket::Status::Done) {
            std::string type;
            common::PlayerId id;
            if (sender==serverIp_ && port==common::SERVER_PORT && (packet >> type >> id) &&
                type=="leave_ack" && id==myId_) { acknowledged=true; break; }
            if (timeout.getElapsedTime()>=sf::milliseconds(300)) break;
        }
        if (!acknowledged) sf::sleep(sf::milliseconds(5));
    }
    myId_=static_cast<common::PlayerId>(-1);
    attackOutbox_.clear();
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

        if (type == common::MSG_WORLD_PART) {
            auto assembled=worldAssembler_.accept(packet);
            if (!assembled) continue;
            packet=std::move(*assembled);
            if (!(packet >> type)) continue;
        }
        if (type == common::MSG_WORLD) {
            if (!common::readWorldPacket(packet, newStates)) {
                logger.log_error("Error reading new world states");
                continue;
            }
            lastWorld_.restart();
            warnedMissingWorld_=false;
            if (myId_<newStates.size() && !newStates[myId_].alive) attackOutbox_.clear();
        }

        // Repeated world snapshots are the authoritative join/leave notification.

    }
    if (!warnedMissingWorld_ && lastWorld_.getElapsedTime()>=sf::seconds(3)) {
        logger.log_error("No complete world update for 3 seconds. Update both server and client; check return UDP traffic/firewall if this persists.");
        warnedMissingWorld_=true;
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
