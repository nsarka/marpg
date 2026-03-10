#pragma once

#include <tmxlite/Map.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <tmxlite/LayerGroup.hpp>
#include <tmxlite/TileLayer.hpp>
#include <tmxlite/detail/Log.hpp>

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <string>
#include <vector>

namespace common {

inline constexpr unsigned short SERVER_PORT = 54000;
inline constexpr int MAX_PLAYERS = 32;
inline constexpr int NUM_COLLECTIBLES = 8;
inline constexpr int TICK_RATE = 64;
inline constexpr float TICK_DT = 1.f / (float)TICK_RATE;

inline constexpr float WINDOW_WIDTH = 1024.f;
inline constexpr float WINDOW_HEIGHT = 768.f;

inline constexpr float PLAYER_RADIUS = 20.f;
inline constexpr float PICKUP_RADIUS = 14.f;
inline constexpr float PLAYER_SPEED = 220.f;
inline constexpr float INTERP_SPEED = 10.f;

inline constexpr const char* MSG_JOIN = "join";
inline constexpr const char* MSG_JOIN_ACK = "join_ack";
inline constexpr const char* MSG_STATE = "state";
inline constexpr const char* MSG_WORLD = "world";

struct PlayerState {
    bool connected = false;
    sf::Vector2f pos{0.f, 0.f};
    std::string name = "Player";
    int score = 0;
};

struct CollectibleState {
    bool active = false;
    sf::Vector2f pos{0.f, 0.f};
};

float distanceSq(const sf::Vector2f& a, const sf::Vector2f& b);
sf::Vector2f lerp(const sf::Vector2f& a, const sf::Vector2f& b, float t);
void clampToPlayfield(sf::Vector2f& pos, float radius = PLAYER_RADIUS);

void writePlayerState(sf::Packet& packet, const PlayerState& player);
bool readPlayerState(sf::Packet& packet, PlayerState& player);

void writeCollectibleState(sf::Packet& packet, const CollectibleState& collectible);
bool readCollectibleState(sf::Packet& packet, CollectibleState& collectible);

void writeWorldPacket(sf::Packet& packet,
                      int connectedCount,
                      const std::vector<PlayerState>& players,
                      const std::vector<CollectibleState>& collectibles);

bool readWorldPacket(sf::Packet& packet,
                     int& connectedCount,
                     std::vector<PlayerState>& players,
                     std::vector<CollectibleState>& collectibles);

void parseTest(const char *map_path);

} // namespace common