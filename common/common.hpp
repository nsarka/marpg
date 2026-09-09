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
#include <cmath>

namespace common {

inline constexpr const char* LEVEL_PATH = "../assets/tiled/legacy_demo.tmx";

inline constexpr unsigned short SERVER_PORT = 54000;
inline constexpr int MAX_PLAYERS = 32;
inline constexpr int TICK_RATE = 64;
inline constexpr float TICK_DT = 1.f / (float)TICK_RATE;
inline constexpr float WALK_SPEED = 120.f;
inline constexpr float RUN_SPEED = 270.f;

inline constexpr float WINDOW_WIDTH = 1024.f;
inline constexpr float WINDOW_HEIGHT = 768.f;

inline constexpr const char* MSG_JOIN = "join";
inline constexpr const char* MSG_JOIN_ACK = "join_ack";
inline constexpr const char* MSG_STATE = "state";
inline constexpr const char* MSG_WORLD = "world";

using PlayerId = std::uint32_t;
using Tick = std::uint32_t;

enum class AttackKind : std::uint8_t {
    None = 0,
    Jab,
    Hook,
    Uppercut,
    Lightning
};

struct AttackDesc {
    std::uint32_t startupTicks;
    std::uint32_t activeTicks;
    std::uint32_t recoveryTicks;

    float range;
    int damage;
};

const AttackDesc& attackDescription(AttackKind kind);
void setAttackDamage(int jab,int hook);

struct AttackState {
    AttackKind kind = AttackKind::None;

    std::uint32_t startTick = 0;

    std::uint32_t activeBeginTick = 0;
    std::uint32_t activeEndTick = 0;

    std::uint32_t recoveryEndTick = 0;

    bool hasHitThisSwing = false;
};

struct InputCommand {
    std::uint32_t sequence = 0;

    sf::Vector2f move{0.f, 0.f};
    sf::Vector2f aim{0.f, 0.f};

    bool sprint = false;

    bool jabHeld = false;
    bool jabPressed = false;
    bool jabReleased = false;

    AttackKind spellKind = AttackKind::Uppercut;
    bool spellPressed = false;
    sf::Vector2f spellTarget{};
    sf::Vector2f cursor{};
    bool hasCursor=false;
    bool movementFacing=false;
    bool hookHeld = false;
    bool hookPressed = false;
    bool hookReleased = false;
};

struct CombatDebugState {
    AttackKind attack = AttackKind::None;
    Tick age = 0;
    sf::Vector2f direction{0.70710678f,-0.70710678f};
    bool hit = false;
    std::int32_t target = -1;
};

struct DamageEvent {
    std::uint32_t sequence = 0;
    int amount = 0;
    std::int32_t source = -1; // -1 means environmental damage.
    sf::Vector2f contact{};
};
inline constexpr std::size_t DamageHistorySize = 8;
enum class KillCause : std::uint8_t { Hit, Jab, Hook, Floor, Bounds, Spell, Lightning };
struct KillEvent {
    std::uint32_t sequence=0;
    std::int32_t killer=-1;
    PlayerId victim=0;
    std::int32_t killerTeam=-1,victimTeam=-1;
    std::string killerName,victimName;
    KillCause cause=KillCause::Hit;
};
inline constexpr std::size_t KillHistorySize=32;

struct PlayerState {
    std::int32_t team=-1;
    bool connected = false;
    bool alive = true;
    sf::Vector2f pos{-140.f, 620.f};
    sf::Vector2f vel{0.f, 0.f};
    std::string name = "Player";
    int health = 100;
    int score = 0;
    std::uint32_t kills = 0, deaths = 0;
    std::uint32_t spellSequence = 0;
    sf::Vector2f spellPosition{};
    AttackKind spellEffect = AttackKind::Uppercut;
    // Retain the last event so a dropped snapshot does not lose the animation.
    AttackKind lastAttack = AttackKind::None;
    std::uint32_t attackSequence = 0;
    CombatDebugState combatDebug;
    Tick stunTicks = 0;
    Tick explosionCooldown = 0, lightningCooldown = 0;
    std::int32_t pingMs = -1;
    sf::Vector2f facing{1.f, 0.f};
    std::uint32_t damageSequence = 0;
    std::vector<DamageEvent> damageEvents;
};

void writeInputCmd(sf::Packet& packet, const common::PlayerId& id, const common::InputCommand& cmd);
bool readInputCmd(sf::Packet& packet, common::PlayerId& id, common::InputCommand& cmd);

void writePlayerState(sf::Packet& packet, const PlayerState& player);
bool readPlayerState(sf::Packet& packet, PlayerState& player);

void writeWorldPacket(sf::Packet& packet,
                      const std::vector<PlayerState>& players, const std::vector<KillEvent>& kills = {});

bool readWorldPacket(sf::Packet& packet,
                     std::vector<PlayerState>& players, std::vector<KillEvent>* kills = nullptr);

void parseTest(const char *map_path);

template<typename T>
T distance(const sf::Vector2<T>& p1, const sf::Vector2<T>& p2) {
    // Calculate the difference between coordinates
    T dx = p2.x - p1.x;
    T dy = p2.y - p1.y;

    // Use std::hypot (C++11) or std::sqrt(dx*dx + dy*dy)
    // std::hypot is generally more robust against overflow
    return std::hypot(dx, dy); 
}

} // namespace common