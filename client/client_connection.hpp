#pragma once

#include "common/common.hpp"
#include "common/world_transport.hpp"
#include "common/settings.hpp"
#include "common/attack_delivery.hpp"
#include "common/logger.hpp"
#include "camera.hpp"
#include "player.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

class ClientConnection {
public:
    explicit ClientConnection(common::Logger& logger);

    [[nodiscard]] common::PlayerId connectToServer(const std::string serverText = "127.0.0.1", const std::string myName = "Rick", unsigned short port=54000);
    ~ClientConnection();
    void leaveServer();

    void pumpNetwork(std::vector<common::PlayerState>& newStates, std::vector<common::PlayerId>& joinedPlayers);

    void sendInput(common::PlayerId& id, common::InputCommand &cmd);

private:
    common::Logger& logger;
    sf::UdpSocket udp_socket_;
    common::PlayerId myId_=static_cast<common::PlayerId>(-1);
    sf::IpAddress serverIp_;
    unsigned short serverPort_=54000;
    common::AttackOutbox attackOutbox_;
    sf::Clock attackClock_;
    common::WorldAssembler worldAssembler_;
    sf::Clock lastWorld_;
    bool warnedMissingWorld_=false;
};