#include "server/game_simulation.hpp"
#include <iostream>
#include <limits>
#include <sstream>

void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() try {
    common::PlayerState player;
    player.connected = true;
    player.pos = {0, 0};
    common::CombatState combat;
    combat.attack = common::AttackKind::Beam;
    combat.bufferedAttack = common::AttackKind::Light;
    combat.bufferedTicks = 5;
    check(common::startDodge(player, combat, {3, 4}), "Cannot start dodge");
    check(std::abs(player.dodgeDirection.length() - 1) < .0001f &&
              combat.attack == common::AttackKind::None && combat.bufferedAttack == common::AttackKind::None,
          "Dodge failed to normalize direction or cancel combat");
    check(!common::startDodge(player, combat, {1, 0}), "Dodge bypassed cooldown");
    check(!common::requestAttack(player, combat, {1, common::AttackKind::Light, {1, 0}, 0}) &&
              combat.bufferedAttack == common::AttackKind::None,
          "Attacks queued during dodge");
    common::CollisionWorld empty;
    for (unsigned tick = 0; tick < common::DodgeDurationTicks; ++tick)
        common::moveDodge(player, empty, {});
    check(std::abs(player.pos.length() - common::DodgeSpeed * common::TICK_DT * common::DodgeDurationTicks) <
              .01f,
          "Dodge distance or diagonal speed is incorrect");
    common::CollisionWorld wall;
    wall.addPolygon({{30, -100}, {40, -100}, {40, 100}, {30, 100}});
    player.pos = {0, 0};
    player.dodgeDirection = {1, 0};
    player.dodgeTicks = common::DodgeDurationTicks;
    for (int tick = 0; tick < 20; ++tick)
        common::moveDodge(player, wall, {});
    check(player.pos.x <= 20.01f && !wall.overlaps(player.pos), "Dodge tunneled through a wall");
    player.pos = {0, 0};
    player.dodgeTicks = common::DodgeDurationTicks;
    for (int tick = 0; tick < 20; ++tick)
        common::moveDodge(player, empty, {{40, 0}});
    check(player.pos.x <= 20.01f, "Dodge passed through another player");
    for (auto kind : {common::AttackKind::Light, common::AttackKind::Heavy, common::AttackKind::Explosion,
                      common::AttackKind::Lightning, common::AttackKind::Beam}) {
        common::PlayerState attacker, target;
        attacker.connected = target.connected = true;
        attacker.team = 0;
        target.team = 1;
        attacker.pos = {0, 0};
        target.pos = {40, 0};
        target.dodgeTicks = common::DodgeDurationTicks;
        common::ServerSettings rules;
        rules.lightWindupSeconds = rules.heavyWindupSeconds = common::TICK_DT;
        rules.spell.windupSeconds = rules.lightning.windupSeconds = rules.beam.windupSeconds =
            common::TICK_DT;
        common::CombatState attack;
        check(common::startAttack(attacker, attack, kind, target.pos, rules), "Combat test attack failed");
        for (int tick = 0; tick < 2; ++tick)
            common::updateAttack(attacker, attack, {&attacker, &target}, empty, {}, rules);
        check(target.health == 100, "Combat damage ignored dodge protection");
        target.dodgeTicks = 0;
        attack = {};
        attacker.explosionCooldown = attacker.lightningCooldown = attacker.beamCooldown = 0;
        attacker.stunTicks = 0;
        check(common::startAttack(attacker, attack, kind, target.pos, rules), "Follow-up attack failed");
        for (int tick = 0; tick < 2; ++tick)
            common::updateAttack(attacker, attack, {&attacker, &target}, empty, {}, rules);
        check(target.health < 100, "Combat protection persisted after dodge ended");
    }
    player.health = 100;
    player.dodgeTicks = common::DodgeDurationTicks;
    common::applyDamage(player, 5, -1, player.pos);
    check(player.health == 95 && player.dodgeTicks == 0, "Dodge incorrectly protected against floor hazards");
    player.dodgeCooldown = 0;
    check(!common::startDodge(player, combat, {1, 0}), "Stunned player could dodge");
    player.stunTicks = 0;
    check(!common::startDodge(player, combat, {std::numeric_limits<float>::quiet_NaN(), 0}),
          "Invalid dodge direction accepted");
    combat.facing = {0, -1};
    check(common::startDodge(player, combat, {}) && player.dodgeDirection == sf::Vector2f{0, -1},
          "Stationary dodge did not fall back to facing");
    sf::Packet statePacket;
    common::writePlayerState(statePacket, player);
    common::PlayerState restored;
    check(common::readPlayerState(statePacket, restored) && restored.dodgeTicks == player.dodgeTicks &&
              restored.dodgeCooldown == player.dodgeCooldown &&
              restored.dodgeSequence == player.dodgeSequence &&
              restored.dodgeDirection == player.dodgeDirection,
          "Dodge state did not replicate");
    common::InputCommand request;
    request.dodgeSequence = 1;
    request.dodgeDirection = {1, 0};
    request.dodgeAgeMs = 0;
    sf::Packet inputPacket;
    common::writeInputCmd(inputPacket, common::PlayerId{0}, request);
    std::string message;
    inputPacket >> message;
    common::PlayerId id;
    common::InputCommand decoded;
    check(common::readInputCmd(inputPacket, id, decoded) && decoded.dodgeSequence == 1 &&
              decoded.dodgeDirection == request.dodgeDirection,
          "Dodge input did not roundtrip");
    std::ostringstream output;
    common::Logger logger(output);
    common::ServerSettings rules;
    rules.map = "city";
    rules.bots = 0;
    rules.botAI = false;
    rules.slots = 2;
    rules.respawnSeconds = common::TICK_DT;
    GameSimulation game(rules, logger);
    check(game.join(0, "Dodger", 1), "Test player failed to join");
    game.players[0].state.pos = {1300, 1300};
    const auto start = game.players[0].state.pos;
    game.input(0, request);
    for (unsigned tick = 0; tick < common::DodgeDurationTicks; ++tick) {
        game.input(0, request); // Repeated request covers movement-packet retries.
        game.step();
    }
    auto& state = game.players[0].state;
    check(state.dodgeSequence == 1 && state.dodgeTicks == 0 &&
              std::abs((state.pos - start).length() -
                       common::DodgeSpeed * common::TICK_DT * common::DodgeDurationTicks) < .05f,
          "Server dodge duration/distance is wrong or retries restarted it");
    request.dodgeSequence = 2;
    game.input(0, request); // Rejected during cooldown and consumed, never queued.
    for (unsigned tick = 0; tick < common::DodgeCooldownTicks; ++tick)
        game.step();
    game.input(0, request);
    check(state.dodgeTicks == 0 && state.dodgeSequence == 1, "Old cooldown request fired later");
    request.dodgeSequence = 3;
    request.dodgeAgeMs = common::AttackBufferMs;
    game.input(0, request);
    check(state.dodgeTicks == 0, "Expired dodge request started a dodge");
    request.dodgeSequence = 4;
    request.dodgeAgeMs = 0;
    game.input(0, request);
    check(state.dodgeTicks == common::DodgeDurationTicks && state.dodgeSequence == 2,
          "New dodge did not start after cooldown");
    state.health = 0;
    state.alive = false;
    request.dodgeSequence = 5;
    game.input(0, request);
    game.step();
    game.input(0, request);
    check(state.alive && state.dodgeTicks == 0 && state.dodgeCooldown == 0,
          "Death/respawn replayed a dodge request");
    game.restartRound();
    game.input(0, request);
    check(game.players[0].state.dodgeTicks == 0, "Round restart replayed a dodge request");
    GameSimulation replacement(rules, logger, nullptr, false);
    replacement.restoreRoster(game);
    replacement.input(0, request);
    check(replacement.players[0].state.dodgeTicks == 0, "Map reload replayed a dodge request");
    std::cout << "PASS: dodge direction/distance, walls/players, combat protection, hazards, cooldown, "
                 "interruption, packet retries, lifecycle and replication\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
