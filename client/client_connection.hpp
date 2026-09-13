#pragma once
#include "common/chat.hpp"
#include "common/load_progress.hpp"
#include "common/runtime_settings.hpp"

#include "camera.hpp"
#include "common/attack_delivery.hpp"
#include "common/common.hpp"
#include "common/logger.hpp"
#include "common/settings.hpp"
#include "common/world_transport.hpp"
#include "player.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

class ClientConnection {
  public:
    const common::ServerSettings& serverSettings() const {
        return settings_;
    }
    explicit ClientConnection(common::Logger& logger);

    [[nodiscard]] common::PlayerId connectToServer(const std::string serverText = "127.0.0.1",
                                                   const std::string myName = "Rick",
                                                   unsigned short port = 54000,
                                                   std::uint32_t requestedTeam = 0,
                                                   common::LoadProgress* progress = nullptr);
    ~ClientConnection();
    void leaveServer();
    const std::string& connectionError() const {
        return connectionError_;
    }
    bool shuttingDown() const {
        return !shutdownReason_.empty();
    }
    const std::string& shutdownReason() const {
        return shutdownReason_;
    }
    bool hasWorldSnapshot() const {
        return hasWorld_;
    }
    const std::vector<common::KillEvent>& killEvents() const {
        return killEvents_;
    }

    void pumpNetwork(std::vector<common::PlayerState>& newStates,
                     std::vector<common::PlayerId>& joinedPlayers);

    std::uint32_t worldVersion() const {
        return runtime_.world;
    }
    bool worldReloadRequested() const {
        return worldReloadRequested_;
    }
    void beginWorldLoad() {
        worldReloadRequested_ = false;
        attackOutbox_.clear();
    }
    void markWorldLoaded(std::uint32_t world);
    bool isBot(common::PlayerId id) const {
        return id < 32 && (runtime_.botMask & (std::uint32_t(1) << id));
    }
    bool sendChat(const std::string& text);
    std::vector<common::ChatMessage> takeChatMessages() {
        return std::exchange(chatMessages_, {});
    }
    bool takeChatFailure() {
        return std::exchange(chatFailed_, false);
    }

    void sendInput(common::PlayerId& id, common::InputCommand& cmd);

  private:
    common::RuntimeSettings runtime_;
    std::uint32_t loadedWorld_ = 1;
    bool worldReloadRequested_ = false;
    common::ChatOutbox chatOutbox_;
    common::ChatInbox chatInbox_;
    sf::Clock chatClock_;
    std::uint32_t nextChatSequence_ = 0;
    std::vector<common::ChatMessage> chatMessages_;
    bool chatFailed_ = false;
    common::ServerSettings settings_;
    std::string connectionError_;
    common::Logger& logger;
    sf::UdpSocket udp_socket_;
    common::PlayerId myId_ = static_cast<common::PlayerId>(-1);
    sf::IpAddress serverIp_;
    unsigned short serverPort_ = 54000;
    common::AttackOutbox attackOutbox_;
    sf::Clock attackClock_;
    common::WorldAssembler worldAssembler_;
    sf::Clock lastWorld_;
    bool warnedMissingWorld_ = false;
    bool hasWorld_ = false;
    std::string shutdownReason_;
    std::vector<common::KillEvent> killEvents_;
};