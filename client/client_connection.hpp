#pragma once

#include "common/common.hpp"
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

    [[nodiscard]] common::PlayerId connectToServer(const std::string serverText = "127.0.0.1", const std::string myName = "Rick");
    void leaveServer();

    void pumpNetwork(std::vector<Player>& players);

    void sendInput(common::InputCommand &cmd);

private:
    common::Logger& logger;
    sf::UdpSocket udp_socket_;
    common::PlayerId myId_;
    sf::IpAddress serverIp_;
};