#pragma once
#include "common.hpp"
#include "settings.hpp"
#include "collision_world.hpp"
#include "attack_delivery.hpp"
#include "damage.hpp"
#include <algorithm>

namespace common {
inline constexpr float AttackHalfAngle = 0.78539816339f; // 90-degree full cone.
inline constexpr float AttackArcCosine = 0.70710678118f;
inline constexpr Tick RespawnDelayTicks = 160; // 2.5 seconds; Die clip is 2.442 seconds.
struct CombatState {
    AttackKind attack = AttackKind::None;
    Tick age = 0;
    bool hit = false;
    std::int32_t hitTarget = -1;
    sf::Vector2f facing{0.70710678f,-0.70710678f}; // Matches the default Dir1 sprite.
    sf::Vector2f attackDirection = facing;
    Tick deadTicks = 0;
    AttackKind bufferedAttack = AttackKind::None;
    sf::Vector2f bufferedAim{};
    Tick bufferedTicks = 0;
};
inline bool startAttack(PlayerState& player, CombatState& combat, AttackKind kind, sf::Vector2f aim = {}) {
    if (!player.alive || player.health<=0 || combat.attack!=AttackKind::None ||
        (kind!=AttackKind::Jab && kind!=AttackKind::Hook)) return false;
    if (!std::isfinite(aim.x) || !std::isfinite(aim.y)) return false;
    const float aimLength=aim.length();
    if (!std::isfinite(aimLength)) return false;
    combat.attack=kind; combat.age=0; combat.hit=false; combat.hitTarget=-1;
    combat.attackDirection=aimLength>0.001f ? aim/aimLength : combat.facing;
    player.lastAttack=kind; ++player.attackSequence;
    return true;
}
inline bool requestAttack(PlayerState& player, CombatState& combat, const AttackRequest& request) {
    if (!player.alive || player.health<=0 || request.ageMs>=AttackBufferMs ||
        (request.kind!=AttackKind::Jab && request.kind!=AttackKind::Hook) ||
        !std::isfinite(request.aim.x) || !std::isfinite(request.aim.y) ||
        !std::isfinite(request.aim.length())) return false;
    if (startAttack(player,combat,request.kind,request.aim)) return true;
    combat.bufferedAttack=request.kind;
    combat.bufferedAim=request.aim.length()>0.001f ? request.aim.normalized() : combat.facing;
    // Account for time already spent awaiting delivery, rather than extending the
    // buffer on every retry. A new click may replace this slot; duplicate IDs may not.
    combat.bufferedTicks=(AttackBufferMs-request.ageMs)*TICK_RATE/1000;
    return combat.bufferedTicks>0;
}
inline bool inAttackArc(sf::Vector2f delta, sf::Vector2f direction, float range) {
    const float distance=delta.length();
    return distance<=range && (distance<=0.001f || delta.dot(direction)>=distance*AttackArcCosine);
}
inline bool attackPathClear(sf::Vector2f from, sf::Vector2f to, const CollisionWorld& walls) {
    return (walls.move(from,to-from,0.f)-to).length()<=0.01f;
}
inline void updateAttack(PlayerState& attacker, CombatState& combat,
                         const std::vector<PlayerState*>& targets, const CollisionWorld& walls) {
    if (!attacker.alive || attacker.health<=0) { combat.attack=AttackKind::None; combat.bufferedAttack=AttackKind::None; combat.bufferedTicks=0; return; }
    if (combat.attack==AttackKind::None) return;
    const auto& desc=attackDescription(combat.attack);
    ++combat.age;
    if (!combat.hit && combat.age>=desc.startupTicks && combat.age<desc.startupTicks+desc.activeTicks) {
        PlayerState* closest=nullptr;
        float best=desc.range;
        for (auto* target : targets) {
            if (target==&attacker || !target->connected || !target->alive || target->health<=0) continue;
            if (!activeSettings.friendlyFire && attacker.team>=0 && attacker.team==target->team) continue;
            const auto delta=target->pos-attacker.pos;
            const float distance=delta.length();
            if (distance>best) continue;
            // A 90-degree forward arc, captured when the swing starts.
            if (!inAttackArc(delta,combat.attackDirection,desc.range)) continue;
            if (!attackPathClear(attacker.pos,target->pos,walls)) continue;
            closest=target; best=distance;
        }
        if (closest) {
            const auto towardAttacker=attacker.pos-closest->pos;
            const auto contact=closest->pos + (towardAttacker.length()>0.001f
                ? towardAttacker.normalized()*CollisionWorld::PlayerRadius : sf::Vector2f{}) + sf::Vector2f{0,-28};
            const auto source=static_cast<std::int32_t>(std::find(targets.begin(),targets.end(),&attacker)-targets.begin());
            applyDamage(*closest,desc.damage,source,contact);
            combat.hit=true;
            combat.hitTarget=static_cast<std::int32_t>(std::find(targets.begin(),targets.end(),closest)-targets.begin());
        }
    }
    if (combat.age>=desc.startupTicks+desc.activeTicks+desc.recoveryTicks)
        combat.attack=AttackKind::None;
    if (combat.bufferedAttack!=AttackKind::None) {
        if (combat.bufferedTicks>0 && combat.attack==AttackKind::None) {
            const auto kind=combat.bufferedAttack;
            const auto aim=combat.bufferedAim;
            combat.bufferedAttack=AttackKind::None;
            combat.bufferedTicks=0;
            startAttack(attacker,combat,kind,aim);
        } else if (combat.bufferedTicks==0 || --combat.bufferedTicks==0) {
            combat.bufferedAttack=AttackKind::None;
        }
    }
}
inline bool advanceDeath(PlayerState& player, CombatState& combat) {
    if (player.health>0 && player.alive) { combat.deadTicks=0; return false; }
    player.health=0; player.alive=false; player.vel={};
    combat.attack=AttackKind::None;
    combat.bufferedAttack=AttackKind::None; combat.bufferedTicks=0;
    if (combat.deadTicks<RespawnDelayTicks) ++combat.deadTicks;
    return combat.deadTicks>=RespawnDelayTicks;
}
inline void respawn(PlayerState& player, CombatState& combat, sf::Vector2f position) {
    player.pos=position; player.vel={}; player.health=100; player.alive=true;
    player.lastAttack=AttackKind::None;
    combat=CombatState{};
}
}
