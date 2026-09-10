#pragma once
#include "attack_delivery.hpp"
#include "collision_world.hpp"
#include "common.hpp"
#include "damage.hpp"
#include "settings.hpp"
#include <algorithm>
#include <random>

namespace common {
inline float attackHalfAngle(AttackKind kind, const ServerSettings& settings = ServerSettings{}) {
    return attackDescription(kind, settings).coneDegrees * 3.141592653589793f / 360.f;
}
inline Tick respawnDelayTicks(const ServerSettings& settings = ServerSettings{}) {
    return static_cast<Tick>(std::llround(settings.respawnSeconds * TICK_RATE));
}
struct CombatState {
    AttackKind attack = AttackKind::None;
    Tick elapsedTicks = 0;
    std::uint32_t damageSequenceAtStart = 0;
    double nextLightningTick = 0;
    bool hit = false;
    std::int32_t hitTarget = -1;
    sf::Vector2f facing{0.70710678f, -0.70710678f}; // Matches the default Dir1 sprite.
    sf::Vector2f attackDirection = facing;
    sf::Vector2f spellTarget{};
    Tick deadTicks = 0;
    AttackKind bufferedAttack = AttackKind::None;
    sf::Vector2f bufferedAim{};
    Tick bufferedTicks = 0;
};
template <typename Combat>
inline bool isInWindup(const Combat& combat, const ServerSettings& settings = ServerSettings{}) {
    return combat.attack != AttackKind::None &&
           combat.elapsedTicks < attackDescription(combat.attack, settings).startupTicks;
}
// Spells may damage their caster; melee attacks never target their attacker.
inline bool canDamageTarget(const PlayerState& attacker, const PlayerState& target, bool allowSelf,
                            const ServerSettings& settings) {
    if (!target.connected || !target.alive)
        return false;
    if (&attacker == &target)
        return allowSelf;
    return settings.friendlyFire || attacker.team < 0 || attacker.team != target.team;
}
inline bool startAttack(PlayerState& player, CombatState& combat, AttackKind kind, sf::Vector2f aim = {},
                        const ServerSettings& settings = ServerSettings{}) {
    if (!player.alive || player.health <= 0 || player.stunTicks > 0 || combat.attack != AttackKind::None ||
        (kind != AttackKind::Light && kind != AttackKind::Heavy && kind != AttackKind::Explosion &&
         kind != AttackKind::Lightning))
        return false;
    if ((kind == AttackKind::Explosion && player.explosionCooldown > 0) ||
        (kind == AttackKind::Lightning && player.lightningCooldown > 0))
        return false;
    if (!std::isfinite(aim.x) || !std::isfinite(aim.y))
        return false;
    if (isSpell(kind)) {
        if ((aim - player.pos).length() > attackDescription(kind, settings).castRange)
            return false;
        combat.spellTarget = aim;
        player.spellPosition = aim;
        aim -= player.pos;
    }
    const float aimLength = aim.length();
    if (!std::isfinite(aimLength))
        return false;
    combat.damageSequenceAtStart = player.damageSequence;
    combat.attack = kind;
    combat.elapsedTicks = 0;
    combat.nextLightningTick = 0;
    combat.hit = false;
    combat.hitTarget = -1;
    combat.attackDirection = aimLength > 0.001f ? aim / aimLength : combat.facing;
    if (isSpell(kind))
        combat.facing = combat.attackDirection;
    player.lastAttack = kind;
    ++player.attackSequence;
    return true;
}
inline bool requestAttack(PlayerState& player, CombatState& combat, const AttackRequest& request,
                          const ServerSettings& settings = ServerSettings{}) {
    if (!player.alive || player.health <= 0 || player.stunTicks > 0 || request.ageMs >= AttackBufferMs ||
        (request.kind != AttackKind::Light && request.kind != AttackKind::Heavy &&
         request.kind != AttackKind::Explosion && request.kind != AttackKind::Lightning) ||
        !std::isfinite(request.aim.x) || !std::isfinite(request.aim.y) ||
        !std::isfinite(request.aim.length()))
        return false;
    if (startAttack(player, combat, request.kind, request.aim, settings))
        return true;
    if (isSpell(request.kind))
        return false; // Spells cannot queue through their cooldown.
    combat.bufferedAttack = request.kind;
    combat.bufferedAim = request.aim.length() > 0.001f ? request.aim.normalized() : combat.facing;
    // Account for time already spent awaiting delivery, rather than extending the
    // buffer on every retry. A new click may replace this slot; duplicate IDs may not.
    combat.bufferedTicks = (AttackBufferMs - request.ageMs) * TICK_RATE / 1000;
    return combat.bufferedTicks > 0;
}
inline bool inAttackArc(sf::Vector2f delta, sf::Vector2f direction, float range,
                        AttackKind kind = AttackKind::Light,
                        const ServerSettings& settings = ServerSettings{}) {
    const float distance = delta.length();
    return distance <= range &&
           (distance <= 0.001f ||
            delta.dot(direction) >= distance * std::cos(attackHalfAngle(kind, settings)));
}
inline bool attackPathClear(sf::Vector2f from, sf::Vector2f to, const CollisionWorld& walls) {
    return (walls.move(from, to - from, 0.f) - to).length() <= 0.01f;
}
inline bool spellTargetValid(const PlayerState& player, AttackKind kind, sf::Vector2f target,
                             const CollisionWorld& walls, const ServerSettings& settings = ServerSettings{}) {
    const auto rules = attackDescription(kind, settings);
    return std::isfinite(target.x) && std::isfinite(target.y) &&
           (target - player.pos).length() <= rules.castRange && attackPathClear(player.pos, target, walls);
}
inline void updateAim(PlayerState& player, CombatState& combat, sf::Vector2f cursor, const CollisionWorld&,
                      const ServerSettings& settings = ServerSettings{}) {
    const bool spellWindup = isSpell(combat.attack) && isInWindup(combat, settings);
    const auto delta = (spellWindup ? combat.spellTarget : cursor) - player.pos;
    const float length = delta.length();
    if (!std::isfinite(length) || length < .001f)
        return;
    combat.facing = delta / length;
    if (combat.attack == AttackKind::None) {
        combat.attackDirection = combat.facing;
        return;
    }
    if (combat.elapsedTicks >= attackDescription(combat.attack, settings).startupTicks)
        return;
    // Spell destinations are captured by startAttack from the initial click.
    if (isSpell(combat.attack))
        return;
    combat.attackDirection = combat.facing;
}
inline bool interruptWindup(CombatState& combat, const ServerSettings& settings = ServerSettings{}) {
    if (!isInWindup(combat, settings))
        return false;
    combat.attack = AttackKind::None;
    combat.elapsedTicks = 0;
    combat.bufferedAttack = AttackKind::None;
    combat.bufferedTicks = 0;
    combat.hit = false;
    combat.hitTarget = -1;
    return true;
}
inline void advanceCooldowns(PlayerState& attacker) {
    if (attacker.stunTicks > 0)
        --attacker.stunTicks;
    if (attacker.explosionCooldown > 0)
        --attacker.explosionCooldown;
    if (attacker.lightningCooldown > 0)
        --attacker.lightningCooldown;
}
template <typename HitTarget>
inline bool resolveSpellPulse(PlayerState& attacker, CombatState& combat,
                              const std::vector<PlayerState*>& targets, const CollisionWorld& walls,
                              const AttackDefinition& desc, const HitTarget& hitTarget,
                              const ServerSettings& settings) {
    const bool lightning = combat.attack == AttackKind::Lightning;
    const auto& rules = desc;
    const double elapsed = combat.elapsedTicks - desc.startupTicks;
    if (lightning && elapsed + 1e-6 < combat.nextLightningTick)
        return false;
    if (lightning)
        combat.nextLightningTick += rules.pulseIntervalSeconds * TICK_RATE;
    if (!attackPathClear(attacker.pos, combat.spellTarget, walls)) {
        combat.hit = true;
        return false;
    }
    combat.hit = true;
    attacker.spellEffect = combat.attack;
    attacker.spellPosition = combat.spellTarget;
    ++attacker.spellSequence;
    const auto source =
        static_cast<std::int32_t>(std::find(targets.begin(), targets.end(), &attacker) - targets.begin());
    for (auto* target : targets) {
        if (!canDamageTarget(attacker, *target, true, settings))
            continue;
        if ((target->pos - combat.spellTarget).length() <= desc.hitRadius &&
            attackPathClear(combat.spellTarget, target->pos, walls)) {
            static std::mt19937 random{std::random_device{}()};
            hitTarget(*target, std::uniform_int_distribution<int>(rules.damageMin, rules.damageMax)(random),
                      source, target->pos + sf::Vector2f{0, -28});
        }
    }

    return true;
}
template <typename HitTarget>
inline void resolveMeleeHit(PlayerState& attacker, CombatState& combat,
                            const std::vector<PlayerState*>& targets, const CollisionWorld& walls,
                            const AttackDefinition& desc, const HitTarget& hitTarget,
                            const ServerSettings& settings) {
    PlayerState* closest = nullptr;
    float best = desc.hitRadius;
    for (auto* target : targets) {
        if (!canDamageTarget(attacker, *target, false, settings) || target->health <= 0)
            continue;
        const auto delta = target->pos - attacker.pos;
        const float distance = delta.length();
        if (distance > best)
            continue;
        // Configured cone around the locked attack direction.
        if (!inAttackArc(delta, combat.attackDirection, desc.hitRadius, combat.attack, settings))
            continue;
        if (!attackPathClear(attacker.pos, target->pos, walls))
            continue;
        closest = target;
        best = distance;
    }
    if (closest) {
        const auto towardAttacker = attacker.pos - closest->pos;
        const auto contact =
            closest->pos +
            (towardAttacker.length() > 0.001f ? towardAttacker.normalized() * CollisionWorld::PlayerRadius
                                              : sf::Vector2f{}) +
            sf::Vector2f{0, -28};
        const auto source =
            static_cast<std::int32_t>(std::find(targets.begin(), targets.end(), &attacker) - targets.begin());
        static std::mt19937 random{std::random_device{}()};
        const int minimum = desc.damageMin;
        hitTarget(*closest, std::uniform_int_distribution<int>(minimum, desc.damageMax)(random), source,
                  contact);
        combat.hit = true;
        combat.hitTarget =
            static_cast<std::int32_t>(std::find(targets.begin(), targets.end(), closest) - targets.begin());
    }
}
inline void finishAttackRecovery(PlayerState& attacker, CombatState& combat, const AttackDefinition& desc) {
    const bool spell = isSpell(combat.attack);
    if (spell && combat.elapsedTicks >= desc.startupTicks + desc.activeTicks) {
        auto& cooldown =
            combat.attack == AttackKind::Lightning ? attacker.lightningCooldown : attacker.explosionCooldown;
        cooldown = desc.recoveryTicks;
        combat.attack = AttackKind::None;
    } else if (!spell && combat.elapsedTicks >= desc.startupTicks + desc.activeTicks + desc.recoveryTicks)
        combat.attack = AttackKind::None;
}
inline void consumeBufferedAttack(PlayerState& attacker, CombatState& combat,
                                  const ServerSettings& settings) {
    if (combat.bufferedAttack != AttackKind::None) {
        if (combat.bufferedTicks > 0 && combat.attack == AttackKind::None) {
            const auto kind = combat.bufferedAttack;
            const auto aim = combat.bufferedAim;
            combat.bufferedAttack = AttackKind::None;
            combat.bufferedTicks = 0;
            startAttack(attacker, combat, kind, aim, settings);
        } else if (combat.bufferedTicks == 0 || --combat.bufferedTicks == 0) {
            combat.bufferedAttack = AttackKind::None;
        }
    }
}
inline void updateAttack(PlayerState& attacker, CombatState& combat, const std::vector<PlayerState*>& targets,
                         const CollisionWorld& walls, const std::vector<CombatState*>& targetCombats = {},
                         const ServerSettings& settings = ServerSettings{}) {
    if (!attacker.alive || attacker.health <= 0) {
        combat.attack = AttackKind::None;
        combat.bufferedAttack = AttackKind::None;
        combat.bufferedTicks = 0;
        return;
    }
    advanceCooldowns(attacker);
    if (combat.attack == AttackKind::None)
        return;
    // All damage sources share damageSequence, including map hazards.
    // Check before advancing so a hit on the final windup tick still cancels.
    if (attacker.damageSequence != combat.damageSequenceAtStart && interruptWindup(combat, settings))
        return;
    // Resolve interruption at the hit, before another player's attack can advance.
    const auto hitTarget = [&](PlayerState& target, int amount, std::int32_t source, sf::Vector2f contact) {
        if (applyDamage(target, amount, source, contact, settings) <= 0)
            return;
        const auto index =
            static_cast<std::size_t>(std::find(targets.begin(), targets.end(), &target) - targets.begin());
        if (index < targetCombats.size() && targetCombats[index])
            interruptWindup(*targetCombats[index], settings);
    };
    const auto& desc = attackDescription(combat.attack, settings);
    if (isSpell(combat.attack) && isInWindup(combat, settings)) {
        const auto delta = combat.spellTarget - attacker.pos;
        if (delta.length() > .001f)
            combat.facing = delta.normalized();
    }
    ++combat.elapsedTicks;
    const bool lightning = combat.attack == AttackKind::Lightning;
    if ((!combat.hit || lightning) && combat.elapsedTicks >= desc.startupTicks &&
        combat.elapsedTicks < desc.startupTicks + desc.activeTicks) {
        if (combat.attack == AttackKind::Explosion || lightning) {
            if (!resolveSpellPulse(attacker, combat, targets, walls, desc, hitTarget, settings))
                return;
        } else {
            resolveMeleeHit(attacker, combat, targets, walls, desc, hitTarget, settings);
        }
    }
    finishAttackRecovery(attacker, combat, desc);
    consumeBufferedAttack(attacker, combat, settings);
}
inline bool advanceDeath(PlayerState& player, CombatState& combat,
                         const ServerSettings& settings = ServerSettings{}) {
    if (player.health > 0 && player.alive) {
        combat.deadTicks = 0;
        return false;
    }
    player.health = 0;
    player.alive = false;
    player.vel = {};
    combat.attack = AttackKind::None;
    combat.bufferedAttack = AttackKind::None;
    combat.bufferedTicks = 0;
    if (combat.deadTicks < respawnDelayTicks(settings))
        ++combat.deadTicks;
    return combat.deadTicks >= respawnDelayTicks(settings);
}
inline void respawn(PlayerState& player, CombatState& combat, sf::Vector2f position) {
    player.pos = position;
    player.vel = {};
    player.health = 100;
    player.alive = true;
    player.stunTicks = 0;
    player.explosionCooldown = player.lightningCooldown = 0;
    player.lastAttack = AttackKind::None;
    combat = CombatState{};
}
} // namespace common
