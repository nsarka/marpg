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
    common::Logger& logger;
    std::mt19937 characterRandom{std::random_device{}()};
    common::LoadedMap world;
    std::unique_ptr<common::Navigation> navigation;
    std::unique_ptr<common::BotAI> botAI;

  public:
    common::ServerSettings settings;
    common::CollisionWorld collision;
    common::TriggerSystem triggers;
    common::TeamSpawns spawns;
    std::vector<GamePlayer> players{common::MAX_PLAYERS};
    common::KillHistory killHistory;
    GameSimulation(common::ServerSettings rules, common::Logger& log,
                   common::LoadProgress* progress = nullptr, bool populateBots = true)
        : logger(log), world(common::mapPath(rules), progress), settings(std::move(rules)) {
        collision.load(world.data(), false, progress);
        common::loading(progress, "Building triggers");
        triggers.load(world.data(), settings, progress);
        common::loading(progress, "Preparing spawn points");
        spawns.load(world.data(), settings.teams, collision, triggers);
        if (settings.botAI && settings.bots > 0) {
            navigation = std::make_unique<common::Navigation>(world.data(), collision, triggers, progress);
            botAI = std::make_unique<common::BotAI>(*navigation, collision, std::random_device{}(), settings);
        }
        for (unsigned i = 0; i < (populateBots ? settings.bots : 0); ++i) {
            common::loading(progress, "Spawning bots", i, settings.bots);
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
    std::uint32_t botMask() const {
        std::uint32_t mask = 0;
        for (unsigned i = 0; i < players.size(); ++i)
            if (players[i].state.connected && !players[i].human)
                mask |= std::uint32_t(1) << i;
        return mask;
    }
    bool hasNavigation() const {
        return bool(navigation);
    }
    std::unique_ptr<common::Navigation> buildNavigation(common::LoadProgress* progress) const {
        return std::make_unique<common::Navigation>(world.data(), collision, triggers, progress);
    }
    void installNavigation(std::unique_ptr<common::Navigation> value) {
        navigation = std::move(value);
    }
    void setAI(bool enabled) {
        if (enabled && settings.bots && !navigation)
            throw std::runtime_error("Navigation is not ready");
        settings.botAI = enabled;
        botAI.reset();
        if (enabled && navigation)
            botAI = std::make_unique<common::BotAI>(*navigation, collision, std::random_device{}(), settings);
        for (unsigned i = 0; i < players.size(); ++i)
            if (!players[i].human) {
                players[i].requestedVelocity = {};
                players[i].state.vel = {};
                players[i].combat = {};
            }
    }
    void setBots(unsigned count) {
        const auto humans = std::count_if(players.begin(), players.end(),
                                          [](const auto& p) { return p.state.connected && p.human; });
        if (count > settings.slots || count + humans > settings.slots)
            throw std::runtime_error("Not enough free slots for that many bots");
        if (count && settings.botAI && !navigation)
            throw std::runtime_error("Navigation is not ready");
        unsigned existing = 0;
        for (const auto& p : players)
            existing += p.state.connected && !p.human;
        // Prepare additions first so a missing spawn does not leave a partial update.
        auto candidate = players;
        for (unsigned i = 0; i < candidate.size() && existing > count; ++i)
            if (candidate[i].state.connected && !candidate[i].human) {
                candidate[i].state.connected = false;
                --existing;
            }
        for (unsigned i = 0; i < settings.slots && existing < count; ++i)
            if (!candidate[i].state.connected) {
                auto team = common::smallestTeam(states(candidate), settings.teams);
                auto spawn = spawns.choose(team, states(candidate), collision, triggers);
                if (!spawn)
                    throw std::runtime_error("No safe bot spawn available");
                auto& p = candidate[i];
                p = GamePlayer{};
                p.state.connected = true;
                p.state.team = settings.teams ? int(team) : -1;
                p.state.name = "Bot " + std::to_string(i + 1);
                p.state.pos = *spawn;
                p.state.character = common::chooseCharacter(p.state.team, characterRandom);
                ++existing;
            }
        for (unsigned i = 0; i < players.size(); ++i)
            if (!players[i].human) {
                if (players[i].state.connected && !candidate[i].state.connected) {
                    remove(i);
                    continue;
                }
                if (!players[i].state.connected && candidate[i].state.connected) {
                    triggers.reset(i);
                    if (botAI)
                        botAI->reset(i);
                    candidate[i].state.attackSequence = players[i].state.attackSequence;
                    candidate[i].state.spellSequence = players[i].state.spellSequence;
                    candidate[i].state.damageSequence = players[i].state.damageSequence;
                    candidate[i].state.teleportSequence = players[i].state.teleportSequence + 1;
                }
                players[i] = std::move(candidate[i]);
            }
        settings.bots = count;
        if (settings.botAI)
            setAI(true);
    }
    void restartRound() {
        auto roster = players;
        auto empty = players;
        for (auto& p : empty)
            p.state.connected = false;
        for (unsigned i = 0; i < roster.size(); ++i)
            if (roster[i].state.connected) {
                auto spawn = spawns.choose(roster[i].state.team, states(empty), collision, triggers);
                if (!spawn)
                    throw std::runtime_error("Insufficient safe spawn space to restart");
                auto& p = empty[i];
                p = GamePlayer{};
                p.state.connected = true;
                p.human = roster[i].human;
                p.state.name = roster[i].state.name;
                p.state.team = roster[i].state.team;
                p.state.character = roster[i].state.character;
                p.state.pos = *spawn;
                p.state.teleportSequence = roster[i].state.teleportSequence + 1;
                p.attackInbox = roster[i].attackInbox;
            }
        players = std::move(empty);
        killHistory = {};
        for (unsigned i = 0; i < players.size(); ++i) {
            triggers.reset(i);
            if (botAI)
                botAI->reset(i);
        }
    }
    void restoreRoster(const GameSimulation& old) {
        const bool honor = settings.honorTeamRequests;
        settings.honorTeamRequests = true;
        for (unsigned i = 0; i < players.size(); ++i)
            if (old.players[i].state.connected) {
                const auto& previous = old.players[i];
                if (!join(i, previous.state.name, previous.state.team < 0 ? 0 : previous.state.team + 1))
                    throw std::runtime_error("New map has insufficient safe spawn space");
                players[i].human = previous.human;
                players[i].state.teleportSequence = previous.state.teleportSequence + 1;
                players[i].attackInbox = previous.attackInbox;
            }
        settings.honorTeamRequests = honor;
    }
    std::string changeTeam(common::PlayerId id, unsigned requested) {
        if (!settings.teams)
            throw std::runtime_error("This server is in free-for-all mode");
        if (requested > settings.teams)
            throw std::runtime_error("Team must be 1-" + std::to_string(settings.teams) + " or auto");
        auto roster = states(players);
        roster[id].connected = false;
        auto team = common::chooseTeam(roster, settings, requested);
        auto& player = players[id];
        if (player.state.team == int(team))
            return "You are already on team " + std::to_string(team + 1) + ".";
        auto spawn = spawns.choose(team, roster, collision, triggers);
        if (!spawn)
            throw std::runtime_error("No safe spawn available for that team");
        player.state.team = int(team);
        player.state.character = common::chooseCharacter(team, characterRandom);
        common::respawn(player.state, player.combat, *spawn);
        ++player.state.teleportSequence;
        player.requestedVelocity = {};
        triggers.reset(id);
        if (botAI)
            for (unsigned i = 0; i < players.size(); ++i)
                botAI->reset(i);
        return "Joined team " + std::to_string(team + 1) +
               (requested && team + 1 != requested ? " (team balance applied)." : ".");
    }
    bool join(common::PlayerId id, const std::string& name, unsigned request) {
        const auto team = common::chooseTeam(states(players), settings, request);
        const auto spawn = spawns.choose(team, states(players), collision, triggers);
        if (!spawn)
            return false;
        auto& player = players[id];
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
        auto& player = players[id];
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
        if (botAI)
            botAI->reset(id);
    }
    void requestAttack(common::PlayerId id, const common::AttackRequest& request) {
        auto& player = players[id];
        if ((common::isSpell(request.kind)) &&
            !common::spellTargetValid(player.state, request.kind, request.aim, collision, settings))
            return;
        if (player.attackInbox.accept(request.sequence))
            common::requestAttack(player.state, player.combat, request, settings);
    }
    void input(common::PlayerId id, common::InputCommand cmd) {
        auto& player = players[id];
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
            const bool spellWindup = common::isSpell(kind) && common::isInWindup(player.combat, settings);
            if (cmd.movementFacing && !spellWindup)
                player.combat.facing = cmd.move.length() > .001f ? cmd.move.normalized() : previousFacing;
        }
        player.requestedVelocity = cmd.move * speed;
        player.lastInputTime.restart();
    }
    void step() {
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& player = players[i];
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

            if (!player.human && settings.botAI && botAI) {
                std::vector<common::PlayerState*> opponents;
                for (auto& other : players)
                    opponents.push_back(&other.state);
                botAI->update(i, player.state, player.combat, opponents);
            }

            triggers.update(i, player.state, collision);
        }
        std::vector<common::PlayerState*> targets;
        std::vector<common::CombatState*> targetCombats;
        for (auto& player : players) {
            targets.push_back(&player.state);
            targetCombats.push_back(&player.combat);
        }
        for (auto& player : players) {
            if (player.state.connected)
                common::updateAttack(player.state, player.combat, targets, collision, targetCombats,
                                     settings);
        }
        killHistory.observe(targets, triggers);
    }
    std::vector<common::PlayerState> snapshot() const {
        auto result = states(players);
        for (std::size_t i = 0; i < players.size(); ++i) {
            const auto& combat = players[i].combat;
            result[i].facing = combat.facing;
            result[i].combatDebug = {combat.attack, combat.elapsedTicks,
                                     combat.attack == common::AttackKind::None ? combat.facing
                                                                               : combat.attackDirection,
                                     combat.hit, combat.hitTarget};
        }
        return result;
    }
};
