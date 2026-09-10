#pragma once
#include "common/bot_ai.hpp"
#include "common/character_roster.hpp"
#include "common/kill_history.hpp"
#include "common/loaded_map.hpp"
#include "common/logger.hpp"
#include "common/team_spawns.hpp"
#include "game_player.hpp"
#include <memory>

class GameSimulation {
    common::Logger &logger;
    std::mt19937 characterRandom{std::random_device{}()};
    common::LoadedMap world;
    std::unique_ptr<common::Navigation> navigation;
    std::unique_ptr<common::BotAI> botAI;

  public:
    const common::ServerSettings settings;
    common::CollisionWorld collision;
    common::TriggerSystem triggers;
    common::TeamSpawns spawns;
    std::vector<GamePlayer> players{common::MAX_PLAYERS};
    common::KillHistory killHistory;
    GameSimulation(common::ServerSettings rules, common::Logger &log)
        : logger(log), world(common::mapPath(rules)), settings(std::move(rules)) {
        collision.load(world.data());
        triggers.load(world.data(), settings);
        spawns.load(world.data(), settings.teams, collision, triggers);
        if (settings.botAI && settings.bots > 0) {
            navigation = std::make_unique<common::Navigation>(world.data(), collision, triggers);
            botAI = std::make_unique<common::BotAI>(*navigation, collision, std::random_device{}(), settings);
        }
        for (unsigned i = 0; i < settings.bots; ++i) {
            auto team = common::smallestTeam(states(players), settings.teams);
            auto spawn = spawns.choose(team, states(players), collision, triggers);
            if (!spawn) {
                logger.log_error("No safe bot spawn");
                throw std::runtime_error("No safe bot spawn");
            }
            players[i].state.connected = true;
            players[i].state.team = settings.teams ? static_cast<int>(team) : -1;
            players[i].state.character = common::chooseCharacter(players[i].state.team, characterRandom);
            players[i].state.name = "Bot " + std::to_string(i + 1);
            players[i].state.pos = *spawn;
        }
    }
    bool join(common::PlayerId id, const std::string &name, unsigned request) {
        const auto team = common::chooseTeam(states(players), settings, request);
        const auto spawn = spawns.choose(team, states(players), collision, triggers);
        if (!spawn)
            return false;
        auto &player = players[id];
        player.state.team = settings.teams ? static_cast<int>(team) : -1;
        player.state.character = common::chooseCharacter(player.state.team, characterRandom);
        player.state.pos = *spawn;
        player.state.connected = true;
        player.human = true;
        player.state.name = name.empty() ? "Player" + std::to_string(id + 1) : name;
        player.state.score = 0;
        return true;
    }
    void remove(common::PlayerId id) {
        auto &player = players[id];
        const auto spellSequence = player.state.spellSequence;
        const auto attackSequence = player.state.attackSequence;
        const auto damageSequence = player.state.damageSequence;
        player.state = common::PlayerState{};
        // Keep event counters monotonic when a slot is reused between snapshots.
        player.state.attackSequence = attackSequence;
        player.state.spellSequence = spellSequence;
        player.state.damageSequence = damageSequence;
        player.combat = {};
        player.attackInbox = {};
        player.requestedVelocity = {};
        player.human = false;
        triggers.reset(id);
    }
    void requestAttack(common::PlayerId id, const common::AttackRequest &request) {
        auto &player = players[id];
        if ((request.kind == common::AttackKind::Explosion ||
             request.kind == common::AttackKind::Lightning) &&
            !common::spellTargetValid(player.state, request.kind, request.aim, collision, settings))
            return;
        if (player.attackInbox.accept(request.sequence))
            common::requestAttack(player.state, player.combat, request, settings);
    }
    void input(common::PlayerId id, common::InputCommand cmd) {
        auto &player = players[id];
        if (!player.state.alive) {
            return;
        }
        const float speed = cmd.sprint ? common::RUN_SPEED : common::WALK_SPEED;
        if (!std::isfinite(cmd.move.x) || !std::isfinite(cmd.move.y))
            return;
        const float length = std::hypot(cmd.move.x, cmd.move.y);
        if (length > 1.f)
            cmd.move /= length;
        if (cmd.hasCursor) {
            const auto previousFacing = player.combat.facing;
            common::updateAim(player.state, player.combat, cmd.cursor, collision, settings);
            const auto kind = player.combat.attack;
            const bool spellWindup =
                (kind == common::AttackKind::Explosion || kind == common::AttackKind::Lightning) &&
                player.combat.age < common::attackDescription(kind, settings).startupTicks;
            if (cmd.movementFacing && !spellWindup)
                player.combat.facing = cmd.move.length() > .001f ? cmd.move.normalized() : previousFacing;
        }
        player.requestedVelocity = cmd.move * speed;
        player.lastInputTime.restart();
    }
    void step() {
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto &player = players[i];
            if (!player.state.connected) {
                continue;
            }

            if (common::advanceDeath(player.state, player.combat, settings)) {
                if (const auto spawn =
                        spawns.choose(player.state.team, states(players), collision, triggers)) {
                    common::respawn(player.state, player.combat, *spawn);
                    player.requestedVelocity = {};
                    triggers.reset(i);
                    if (botAI)
                        botAI->reset(i);
                }
                continue;
            }
            if (!player.state.alive) {
                player.requestedVelocity = {};
                triggers.reset(i);
                if (botAI)
                    botAI->reset(i);
                continue;
            }
            std::vector<sf::Vector2f> blockers;
            for (int j = 0; j < common::MAX_PLAYERS; ++j) {
                if (j != i && players[j].state.connected && players[j].state.alive)
                    blockers.push_back(players[j].state.pos);
            }
            const auto oldPosition = player.state.pos;
            if (player.human) {
                const auto velocity = player.lastInputTime.getElapsedTime().asSeconds() < 0.25f
                                          ? player.requestedVelocity
                                          : sf::Vector2f{};
                player.state.pos = collision.move(oldPosition, velocity * common::TICK_DT,
                                                  common::CollisionWorld::PlayerRadius, blockers);
                player.state.vel = (player.state.pos - oldPosition) / common::TICK_DT;
            }

            if (i < static_cast<int>(settings.bots) && settings.botAI) {
                std::vector<common::PlayerState *> opponents;
                for (auto &other : players)
                    opponents.push_back(&other.state);
                botAI->update(i, player.state, player.combat, opponents);
            }

            triggers.update(i, player.state, collision);
        }
        std::vector<common::PlayerState *> targets;
        std::vector<common::CombatState *> targetCombats;
        for (auto &player : players) {
            targets.push_back(&player.state);
            targetCombats.push_back(&player.combat);
        }
        for (auto &player : players) {
            if (player.state.connected)
                common::updateAttack(player.state, player.combat, targets, collision, targetCombats,
                                     settings);
        }
        killHistory.observe(targets, triggers);
    }
    std::vector<common::PlayerState> snapshot() const {
        auto result = states(players);
        for (std::size_t i = 0; i < players.size(); ++i) {
            const auto &combat = players[i].combat;
            result[i].facing = combat.facing;
            result[i].combatDebug = {combat.attack, combat.age,
                                     combat.attack == common::AttackKind::None ? combat.facing
                                                                               : combat.attackDirection,
                                     combat.hit, combat.hitTarget};
        }
        return result;
    }
};
