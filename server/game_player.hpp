#pragma once
#include "common/combat_system.hpp"
#include <SFML/System/Clock.hpp>
struct GamePlayer {
    common::PlayerState state;
    common::CombatState combat;
    common::AttackInbox attackInbox;
    sf::Vector2f requestedVelocity{};
    sf::Clock lastInputTime;
    bool human = false;
};
inline std::vector<common::PlayerState> states(const std::vector<GamePlayer>& players) {
    std::vector<common::PlayerState> result;
    for (const auto& p : players)
        result.push_back(p.state);
    return result;
}
