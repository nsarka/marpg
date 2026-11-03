#pragma once
#include "bot_roles.hpp"
#include "combat_system.hpp"
#include "navigation.hpp"
#include <deque>
#include <random>

namespace common {
class BotAI {
  public:
    BotAI(const Navigation& navigation, const CollisionWorld& walls, unsigned seed = std::random_device{}(),
          const ServerSettings& settings = ServerSettings{})
        : settings_(settings), navigation_(navigation), walls_(walls), random_(seed) {}
    void reset(PlayerId id) {
        brains_[id] = Brain{};
    }
    int target(PlayerId id) const {
        return brains_[id].target;
    }
    void update(PlayerId id, PlayerState& bot, CombatState& combat,
                const std::vector<PlayerState*>& players) {
        auto& brain = brains_[id];
        if (!bot.connected || !bot.alive) {
            reset(id);
            return;
        }
        auto enemy = [&](int i) {
            return i >= 0 && std::size_t(i) < players.size() && players[i] != &bot && players[i]->connected &&
                   players[i]->alive && players[i]->health > 0 &&
                   (settings_.teams == 0 || (players[i]->team >= 0 && players[i]->team != bot.team));
        };
        for (const auto& hit : bot.damageEvents) {
            if (!sequenceNewer(hit.sequence, seenDamage_[id]))
                continue;
            seenDamage_[id] = hit.sequence;
            if (hit.amount <= 0 || !enemy(hit.source))
                continue;
            bool claimed = false;
            for (std::size_t other = 0; other < players.size(); ++other)
                if (other != id && players[other]->connected && players[other]->alive &&
                    brains_[other].target == hit.source) {
                    claimed = true;
                    break;
                }
            if (!claimed) {
                const auto chargeCooldown = brain.chargeCooldown;
                brain = Brain{};
                brain.chargeCooldown = chargeCooldown;
                brain.target = hit.source;
            }
        }
        if (!enemy(brain.target) || ++brain.targetAge >= 12 * TICK_RATE) {
            std::vector<int> enemies;
            for (std::size_t i = 0; i < players.size(); ++i)
                if (enemy(i))
                    enemies.push_back(i);
            const auto chargeCooldown = brain.chargeCooldown;
            brain = Brain{};
            brain.chargeCooldown = chargeCooldown;
            if (enemies.empty()) {
                bot.vel = {};
                return;
            }
            brain.target =
                enemies[std::uniform_int_distribution<std::size_t>(0, enemies.size() - 1)(random_)];
        }
        auto& victim = *players[brain.target];
        auto delta = victim.pos - bot.pos;
        float distance = delta.length();
        std::vector<sf::Vector2f> blockers, traffic;
        for (auto* p : players)
            if (p != &bot && p->connected && p->alive) {
                blockers.push_back(p->pos);
                if (p != &victim)
                    traffic.push_back(p->pos);
            }
        updateAim(bot, combat, victim.pos, walls_, settings_);
        if (brain.pause)
            --brain.pause;
        const bool clear = attackPathClear(bot.pos, victim.pos, walls_);
        const auto role = botRole(id);
        if (brain.chargeCooldown)
            --brain.chargeCooldown;
        if (role == BotRole::Charger && updateCharge(bot, combat, brain, victim.pos, clear, blockers))
            return;
        const bool caster = role == BotRole::Caster;
        const float castRange = float(std::max(settings_.spell.castRange, settings_.lightning.castRange));
        const float retreatRange = std::min(180.f, castRange * .45f);
        const float holdRange = std::min(300.f, castRange * .75f);
        if (caster && combat.attack == AttackKind::None && !brain.pause && clear) {
            constexpr std::array<AttackKind, 3> spells{AttackKind::Explosion, AttackKind::Lightning,
                                                       AttackKind::Beam};
            for (unsigned offset = 0; offset < spells.size(); ++offset) {
                const auto index = (brain.spellChoice + offset) % spells.size();
                const auto kind = spells[index];
                if (spellTargetValid(bot, kind, victim.pos, walls_, settings_) &&
                    startAttack(bot, combat, kind, victim.pos, settings_)) {
                    brain.spellChoice = (index + 1) % spells.size();
                    brain.pause = TICK_RATE / 2;
                    break;
                }
            }
        }
        if (!caster && brain.combo.empty() && combat.attack == AttackKind::None && !brain.pause) {
            const int count = std::uniform_int_distribution<int>(2, 4)(random_);
            const std::array<AttackKind, 3> choices{AttackKind::Light, AttackKind::Heavy,
                                                    role == BotRole::Bruiser ? AttackKind::Heavy
                                                                             : AttackKind::Light};
            for (int i = 0; i < count; ++i)
                brain.combo.push_back(choices[std::uniform_int_distribution<int>(0, 2)(random_)]);
        }
        if (!brain.combo.empty() && combat.attack == AttackKind::None && !brain.pause && clear) {
            const auto kind = brain.combo.front();
            const bool spell = isSpell(kind);
            const auto definition = attackDescription(kind, settings_);
            const float range = definition.area ? definition.castRange : std::min(58.f, definition.hitRadius);
            if (distance <= range && startAttack(bot, combat, kind, spell ? victim.pos : delta, settings_)) {
                brain.combo.pop_front();
                if (brain.combo.empty())
                    brain.pause = TICK_RATE + std::uniform_int_distribution<int>(8, 24)(random_);
            }
        }
        // Stay at the casting position through windup/channeling and recovery.
        if (isSpell(combat.attack) || (clear && distance <= 50)) {
            if (caster && !isSpell(combat.attack) && distance < retreatRange &&
                retreat(bot, victim.pos, blockers, id)) {
                brain.repath = 0;
                return;
            }
            bot.vel = {};
            brain.stuck = 0;
            return;
        }
        if (caster && clear && distance < retreatRange) {
            if (!retreat(bot, victim.pos, blockers, id))
                bot.vel = {};
            brain.repath = 0;
            return;
        }
        if (caster && clear && distance >= retreatRange && distance <= holdRange) {
            bot.vel = {};
            brain.repath = 0;
            return;
        }
        if (brain.repath)
            --brain.repath;
        if (!brain.repath) {
            brain.path = navigation_.path(bot.pos, victim.pos, traffic);
            brain.waypoint = 0;
            brain.repath = TICK_RATE / 2 + id % 9;
            if (brain.path.empty()) {
                bot.vel = {};
                if (++brain.failures >= 3) {
                    brain.target = -1;
                    brain.failures = 0;
                }
                return;
            }
            brain.failures = 0;
        }
        if (brain.path.empty()) {
            bot.vel = {};
            return;
        }
        while (brain.waypoint < brain.path.size() && (brain.path[brain.waypoint] - bot.pos).length() < 8)
            ++brain.waypoint;
        // Short lookahead smooths grid corners without cutting through walls or
        // hazards.
        const auto lookahead = std::min(brain.path.size(), brain.waypoint + 5);
        for (std::size_t next = brain.waypoint + 1; next < lookahead; ++next) {
            bool occupied = false;
            auto segment = brain.path[next] - bot.pos;
            float length = segment.lengthSquared();
            for (auto p : traffic) {
                float t = length > 0 ? std::clamp((p - bot.pos).dot(segment) / length, 0.f, 1.f) : 0;
                if ((bot.pos + segment * t - p).length() < 28) {
                    occupied = true;
                    break;
                }
            }
            if (occupied || !navigation_.clear(bot.pos, brain.path[next]))
                break;
            brain.waypoint = next;
        }
        if (brain.waypoint >= brain.path.size()) {
            bot.vel = {};
            brain.repath = 0;
            return;
        }
        auto toward = brain.path[brain.waypoint] - bot.pos;
        float length = toward.length();
        if (length < .001f) {
            bot.vel = {};
            return;
        }
        auto step = toward / length * std::min(length, RUN_SPEED * TICK_DT);
        auto next = walls_.move(bot.pos, step, CollisionWorld::PlayerRadius, blockers);
        if ((next - bot.pos).length() < step.length() * .25f) {
            ++brain.stuck;
            // Sidestep crowds, using the same collision and floor safety as normal
            // movement.
            sf::Vector2f side{-step.y, step.x};
            if (id % 2)
                side = -side;
            for (auto alternative : {step * .3f + side, step * .3f - side}) {
                if (alternative.length() > step.length())
                    alternative = alternative.normalized() * step.length();
                auto candidate = walls_.move(bot.pos, alternative, CollisionWorld::PlayerRadius, blockers);
                if ((candidate - bot.pos).length() > (next - bot.pos).length() &&
                    navigation_.clear(bot.pos, candidate))
                    next = candidate;
            }
            if (brain.stuck >= TICK_RATE / 4) {
                brain.repath = 0;
                brain.stuck = 0;
            }
        } else
            brain.stuck = 0;
        if (!navigation_.clear(bot.pos, next))
            next = bot.pos;
        bot.vel = (next - bot.pos) / TICK_DT;
        bot.pos = next;
    }

  private:
    ServerSettings settings_;
    struct Brain {
        int target = -1;
        Tick targetAge = 0, repath = 0, pause = 0, stuck = 0;
        unsigned failures = 0;
        std::vector<sf::Vector2f> path;
        std::size_t waypoint = 0;
        std::deque<AttackKind> combo;
        unsigned spellChoice = 0;
        Tick chargeWindup = 0, chargeTicks = 0, chargeCooldown = 0;
        std::uint32_t chargeDamageSequence = 0;
        sf::Vector2f chargeDestination{};
    };
    // Retreat candidates fan out so a nearby obstacle does not pin a caster
    // unnecessarily.
    bool retreat(PlayerState& bot, sf::Vector2f target, const std::vector<sf::Vector2f>& blockers,
                 PlayerId id) const {
        auto away = bot.pos - target;
        away = away.length() > .001f ? away.normalized() : sf::Vector2f{1.f, 0.f};
        sf::Vector2f side{-away.y, away.x};
        if (id % 2)
            side = -side;
        for (auto direction : {away, away + side, away - side, side, -side}) {
            const auto step = direction.normalized() * RUN_SPEED * TICK_DT;
            const auto next = walls_.move(bot.pos, step, CollisionWorld::PlayerRadius, blockers);
            if ((next - target).length() > (bot.pos - target).length() &&
                (next - bot.pos).length() > step.length() * .25f && navigation_.clear(bot.pos, next)) {
                bot.vel = (next - bot.pos) / TICK_DT;
                bot.pos = next;
                return true;
            }
        }
        return false;
    }
    bool updateCharge(PlayerState& bot, CombatState& combat, Brain& brain, sf::Vector2f target, bool clear,
                      const std::vector<sf::Vector2f>& blockers) {
        if ((brain.chargeWindup || brain.chargeTicks) &&
            (bot.stunTicks || bot.damageSequence != brain.chargeDamageSequence)) {
            brain.chargeWindup = brain.chargeTicks = 0;
            brain.repath = 0;
        }
        const float distance = (target - bot.pos).length();
        if (!brain.chargeCooldown && combat.attack == AttackKind::None && !bot.stunTicks && clear &&
            distance >= 140.f && distance <= 320.f && navigation_.clear(bot.pos, target)) {
            brain.chargeDestination = target;
            brain.chargeWindup = TICK_RATE / 2;
            brain.chargeCooldown = 4 * TICK_RATE;
            brain.chargeDamageSequence = bot.damageSequence;
            brain.combo.clear();
        }
        if (brain.chargeWindup) {
            updateAim(bot, combat, brain.chargeDestination, walls_, settings_);
            bot.vel = {};
            if (!--brain.chargeWindup)
                brain.chargeTicks = TICK_RATE * 3 / 4;
            return true;
        }
        if (!brain.chargeTicks)
            return false;
        updateAim(bot, combat, brain.chargeDestination, walls_, settings_);
        const auto delta = brain.chargeDestination - bot.pos;
        if (delta.length() <= 50.f || (clear && distance <= 50.f)) {
            brain.chargeTicks = 0;
            brain.repath = 0;
            bot.vel = {};
            if (clear && distance <= std::min(58.f, float(settings_.heavyRange)))
                startAttack(bot, combat, AttackKind::Heavy, target - bot.pos, settings_);
            return true;
        }
        // Fixed destination, swept collision, and floor checks make the rush
        // dodgeable and safe.
        const auto step = delta.normalized() * std::min(delta.length() - 50.f, RUN_SPEED * 1.8f * TICK_DT);
        const auto next = walls_.move(bot.pos, step, CollisionWorld::PlayerRadius, blockers);
        if (!navigation_.clear(bot.pos, next) || (next - bot.pos).length() < step.length() * .25f) {
            brain.chargeTicks = 0;
            brain.repath = 0;
            bot.vel = {};
            return true;
        }
        bot.vel = (next - bot.pos) / TICK_DT;
        bot.pos = next;
        --brain.chargeTicks;
        brain.repath = 0;
        return true;
    }
    const Navigation& navigation_;
    const CollisionWorld& walls_;
    std::mt19937 random_;
    std::array<Brain, MAX_PLAYERS> brains_;
    std::array<std::uint32_t, MAX_PLAYERS> seenDamage_{};
};
} // namespace common
