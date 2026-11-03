#pragma once
#include "combat_system.hpp"

namespace common {
inline bool startDodge(PlayerState& player, CombatState& combat, sf::Vector2f direction) {
    if (!player.connected || !player.alive || player.health <= 0 || player.stunTicks ||
        player.dodgeCooldown || player.dodgeTicks || !std::isfinite(direction.x) ||
        !std::isfinite(direction.y))
        return false;
    if (direction.length() < .001f)
        direction = combat.facing;
    const float length = direction.length();
    if (!std::isfinite(length) || length < .001f)
        return false;
    combat.attack = combat.bufferedAttack = AttackKind::None;
    combat.elapsedTicks = combat.bufferedTicks = 0;
    combat.hit = false;
    combat.hitTarget = -1;
    player.dodgeDirection = direction / length;
    player.dodgeTicks = DodgeDurationTicks;
    player.dodgeCooldown = DodgeCooldownTicks;
    ++player.dodgeSequence;
    combat.facing = player.dodgeDirection;
    player.facing = player.dodgeDirection;
    return true;
}
inline void moveDodge(PlayerState& player, const CollisionWorld& walls,
                      const std::vector<sf::Vector2f>& blockers) {
    const auto old = player.pos;
    player.pos =
        walls.move(old, player.dodgeDirection * DodgeSpeed * TICK_DT, CollisionWorld::PlayerRadius, blockers);
    player.vel = (player.pos - old) / TICK_DT;
    if ((player.pos - old).length() < .01f)
        player.dodgeTicks = 0;
}
} // namespace common
