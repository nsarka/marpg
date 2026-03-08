#include "common/common.hpp"

#include <algorithm>

namespace common {

float distanceSq(const sf::Vector2f& a, const sf::Vector2f& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

sf::Vector2f lerp(const sf::Vector2f& a, const sf::Vector2f& b, float t) {
    return a + (b - a) * t;
}

void clampToPlayfield(sf::Vector2f& pos, float radius) {
    pos.x = std::clamp(pos.x, radius, WINDOW_WIDTH - radius);
    pos.y = std::clamp(pos.y, radius, WINDOW_HEIGHT - radius);
}

void writePlayerState(sf::Packet& packet, const PlayerState& player) {
    packet << player.connected
           << player.pos.x
           << player.pos.y
           << player.name
           << player.score;
}

bool readPlayerState(sf::Packet& packet, PlayerState& player) {
    float x = 0.f;
    float y = 0.f;
    if (!(packet >> player.connected >> x >> y >> player.name >> player.score)) {
        return false;
    }

    player.pos = {x, y};
    return true;
}

void writeCollectibleState(sf::Packet& packet, const CollectibleState& collectible) {
    packet << collectible.active
           << collectible.pos.x
           << collectible.pos.y;
}

bool readCollectibleState(sf::Packet& packet, CollectibleState& collectible) {
    float x = 0.f;
    float y = 0.f;
    if (!(packet >> collectible.active >> x >> y)) {
        return false;
    }

    collectible.pos = {x, y};
    return true;
}

void writeWorldPacket(sf::Packet& packet,
                      int connectedCount,
                      const std::vector<PlayerState>& players,
                      const std::vector<CollectibleState>& collectibles) {
    packet << std::string(MSG_WORLD);
    packet << connectedCount;

    packet << static_cast<int>(players.size());
    for (const auto& player : players) {
        writePlayerState(packet, player);
    }

    packet << static_cast<int>(collectibles.size());
    for (const auto& collectible : collectibles) {
        writeCollectibleState(packet, collectible);
    }
}

bool readWorldPacket(sf::Packet& packet,
                     int& connectedCount,
                     std::vector<PlayerState>& players,
                     std::vector<CollectibleState>& collectibles) {
    int playerCount = 0;
    int collectibleCount = 0;

    if (!(packet >> connectedCount >> playerCount)) {
        return false;
    }
    if (playerCount < 0) {
        return false;
    }

    players.resize(static_cast<std::size_t>(playerCount));
    for (int i = 0; i < playerCount; ++i) {
        if (!readPlayerState(packet, players[static_cast<std::size_t>(i)])) {
            return false;
        }
    }

    if (!(packet >> collectibleCount)) {
        return false;
    }
    if (collectibleCount < 0) {
        return false;
    }

    collectibles.resize(static_cast<std::size_t>(collectibleCount));
    for (int i = 0; i < collectibleCount; ++i) {
        if (!readCollectibleState(packet, collectibles[static_cast<std::size_t>(i)])) {
            return false;
        }
    }

    return true;
}

} // namespace common