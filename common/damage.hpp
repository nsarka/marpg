#pragma once
#include "common.hpp"
#include "settings.hpp"
#include <algorithm>
namespace common {
inline int applyDamage(PlayerState& player, int amount, std::int32_t source, sf::Vector2f contact) {
    if (!player.connected || !player.alive || amount<=0 || player.health<=0) return 0;
    const int dealt=std::min(amount,player.health);
    player.health-=dealt;
    player.stunTicks=static_cast<Tick>(std::llround(activeSettings.damageStunSeconds*TICK_RATE));
    player.damageEvents.push_back({++player.damageSequence,dealt,source,contact});
    if (player.damageEvents.size()>DamageHistorySize) player.damageEvents.erase(player.damageEvents.begin());
    if (player.health==0) { player.alive=false; player.vel={}; }
    return dealt;
}
}
