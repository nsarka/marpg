#include "client/melee_animation.hpp"
#include "common/combat_system.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
common::ServerSettings unstunnedRules() {
    common::ServerSettings rules;
    rules.damageStunSeconds = 0;
    rules.lightConeDegrees = rules.heavyConeDegrees = 90;
    rules.lightRange = 70;
    rules.heavyRange = 80;
    return rules;
}
int main() {
    common::ServerSettings rulesUnderTest;
    rulesUnderTest = common::ServerSettings{};
    check(common::inAttackArc({60, 90}, {1, 0}, 140, common::AttackKind::Light, rulesUnderTest),
          "120-degree light should include a 56-degree target");
    check(!common::inAttackArc({60, 110}, {1, 0}, 140, common::AttackKind::Light, rulesUnderTest),
          "Light should exclude targets beyond 60 degrees");
    check(common::inAttackArc({150, 20}, {1, 0}, 160, common::AttackKind::Heavy, rulesUnderTest),
          "Extended narrow heavy should hit");
    check(!common::inAttackArc({150, 30}, {1, 0}, 160, common::AttackKind::Heavy, rulesUnderTest),
          "Heavy should exclude targets beyond 10 degrees");

    rulesUnderTest = unstunnedRules();
    {
        common::CollisionWorld empty;
        for (auto kind : {common::AttackKind::Light, common::AttackKind::Heavy, common::AttackKind::Explosion,
                          common::AttackKind::Lightning}) {
            common::PlayerState hitter, victim;
            hitter.connected = victim.connected = true;
            hitter.pos = {0, 0};
            victim.pos = {30, 0};
            common::CombatState hit, windup;
            common::startAttack(hitter, hit, common::AttackKind::Light, {1, 0}, rulesUnderTest);
            common::startAttack(victim, windup, kind, {0, 0}, rulesUnderTest);
            windup.bufferedAttack = common::AttackKind::Heavy;
            windup.bufferedTicks = 10;
            hit.elapsedTicks = common::attackDescription(hit.attack, rulesUnderTest).startupTicks - 1;
            common::updateAttack(hitter, hit, {&hitter, &victim}, empty, {&hit, &windup}, rulesUnderTest);
            check(victim.health == 80 && windup.attack == common::AttackKind::None,
                  "Hit must cancel any victim windup");
            check(windup.bufferedAttack == common::AttackKind::None,
                  "Interrupted attack must clear queued swing");
            common::updateAttack(victim, windup, {&hitter, &victim}, empty, {&hit, &windup}, rulesUnderTest);
            check(hitter.health == 100 && victim.spellSequence == 0,
                  "Canceled windup must not deal damage or emit spell");
            common::startAttack(victim, windup, kind, {0, 0}, rulesUnderTest);
            windup.elapsedTicks = common::attackDescription(kind, rulesUnderTest).startupTicks;
            check(!common::interruptWindup(windup, rulesUnderTest) && windup.attack == kind,
                  "Active attacks must not be interrupted");
            windup.elapsedTicks += common::attackDescription(kind, rulesUnderTest).activeTicks;
            check(!common::interruptWindup(windup, rulesUnderTest), "Recovery must not be interrupted");
        }
        // Spell damage goes through the same interruption path.
        common::PlayerState caster, victim;
        caster.connected = victim.connected = true;
        caster.pos = {0, 0};
        victim.pos = {200, 0};
        common::CombatState cast, windup;
        common::startAttack(caster, cast, common::AttackKind::Explosion, victim.pos, rulesUnderTest);
        common::startAttack(victim, windup, common::AttackKind::Heavy, {-1, 0}, rulesUnderTest);
        cast.elapsedTicks = common::attackDescription(cast.attack, rulesUnderTest).startupTicks - 1;
        common::updateAttack(caster, cast, {&caster, &victim}, empty, {&cast, &windup}, rulesUnderTest);
        check(victim.health < 100 && windup.attack == common::AttackKind::None,
              "Spell hits must interrupt windup");
    }
    {
        auto rules = unstunnedRules();
        rules.lightDamageMin = 3;
        rules.lightDamageMax = 9;
        rules.lightConeDegrees = 30;
        rules.heavyConeDegrees = 180;
        rulesUnderTest = rules;
        check(!common::inAttackArc({30, 20}, {1, 0}, 70, common::AttackKind::Light, rulesUnderTest),
              "Narrow light cone ignored");
        check(common::inAttackArc({30, 20}, {1, 0}, 80, common::AttackKind::Heavy, rulesUnderTest),
              "Wide heavy cone ignored");
        common::CollisionWorld open;
        bool varied = false;
        int previous = -1;
        for (int i = 0; i < 100; ++i) {
            common::PlayerState a, b;
            a.connected = b.connected = true;
            a.pos = {0, 0};
            b.pos = {30, 0};
            common::CombatState c;
            common::startAttack(a, c, common::AttackKind::Light, {1, 0}, rulesUnderTest);
            c.elapsedTicks = 15;
            common::updateAttack(a, c, {&a, &b}, open, {}, rulesUnderTest);
            const int damage = 100 - b.health;
            check(damage >= 3 && damage <= 9, "Melee damage outside configured range");
            if (previous >= 0 && previous != damage)
                varied = true;
            previous = damage;
        }
        check(varied, "Melee damage did not vary");
        rulesUnderTest = unstunnedRules();
    }
    {
        struct Frame {
            float durationSeconds = .083f;
        };
        std::vector<Frame> frames(12);
        for (double seconds : {0.0, .25, 1.0, 3.0}) {
            auto rules = unstunnedRules();
            rules.lightWindupSeconds = rules.heavyWindupSeconds = seconds;
            rulesUnderTest = rules;
            for (auto kind : {common::AttackKind::Light, common::AttackKind::Heavy}) {
                float sum = 0;
                for (std::size_t i = 0; i < client::meleeImpactFrame(kind); ++i) {
                    const float duration = client::meleeFrameDuration(kind, frames, i, rulesUnderTest);
                    check(duration > 0, "Melee frame timing must stay positive");
                    sum += duration;
                }
                check(std::abs(sum - common::attackDescription(kind, rulesUnderTest).startupTicks *
                                         common::TICK_DT) < .00001f,
                      "Punch pose must land at configured windup");
                check(client::meleeFrameDuration(kind, frames, 9, rulesUnderTest) == .083f,
                      "Recovery frame timing must stay unchanged");
                if (seconds >= 1)
                    check(client::meleeFrameDuration(kind, frames, 2, rulesUnderTest) > .083f,
                          "Middle windup frames must stretch");
            }
        }
        rulesUnderTest = unstunnedRules();
    }
    {
        common::CollisionWorld empty;
        for (auto kind : {common::AttackKind::Light, common::AttackKind::Heavy, common::AttackKind::Explosion,
                          common::AttackKind::Lightning}) {
            common::PlayerState p;
            p.connected = true;
            p.pos = {0, 0};
            common::CombatState c;
            common::applyDamage(p, 2, -1, p.pos, rulesUnderTest);
            common::startAttack(p, c, kind, {100, 0}, rulesUnderTest);
            common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
            check(c.attack == kind, "Damage before attack must not cancel new windup");
            c.elapsedTicks = common::attackDescription(kind, rulesUnderTest).startupTicks - 1;
            c.bufferedAttack = common::AttackKind::Light;
            c.bufferedTicks = 10;
            common::applyDamage(p, 2, -1, p.pos, rulesUnderTest);
            common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
            check(c.attack == common::AttackKind::None && c.bufferedAttack == common::AttackKind::None &&
                      p.spellSequence == 0,
                  "Environmental damage must cancel windup before activation");
            common::startAttack(p, c, kind, {100, 0}, rulesUnderTest);
            common::applyDamage(p, 0, -1, p.pos, rulesUnderTest);
            common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
            check(c.attack == kind, "Zero damage must not interrupt");
        }
    }
    common::CollisionWorld walls;
    common::PlayerState attacker, target, other;
    attacker.connected = target.connected = other.connected = true;
    attacker.pos = {0, 0};
    target.pos = {30, 0};
    other.pos = {50, 0};
    common::CombatState combat;
    combat.facing = {1, 0};
    std::vector<common::PlayerState*> targets{&attacker, &target, &other};
    check(common::startAttack(attacker, combat, common::AttackKind::Light, {}, rulesUnderTest),
          "Light must start");
    check(!common::startAttack(attacker, combat, common::AttackKind::Heavy, {}, rulesUnderTest),
          "No attack during cooldown");
    for (int i = 0; i < 15; ++i)
        common::updateAttack(attacker, combat, targets, walls, {}, rulesUnderTest);
    check(target.health == 100, "No damage during windup");
    common::updateAttack(attacker, combat, targets, walls, {}, rulesUnderTest);
    check(target.health == 80 && other.health == 100, "Light hits nearest target once for 20");
    for (int i = 0; i < 48; ++i)
        common::updateAttack(attacker, combat, targets, walls, {}, rulesUnderTest);
    check(target.health == 80 && combat.attack == common::AttackKind::None,
          "One hit per swing, cooldown completes");
    common::startAttack(attacker, combat, common::AttackKind::Heavy, {}, rulesUnderTest);
    for (int i = 0; i < 64; ++i)
        common::updateAttack(attacker, combat, targets, walls, {}, rulesUnderTest);
    check(target.health == 45, "Heavy does 35 damage");
    target.pos = {-30, 0};
    other.pos = {1000, 0};
    common::startAttack(attacker, combat, common::AttackKind::Light, {}, rulesUnderTest);
    for (int i = 0; i < 64; ++i)
        common::updateAttack(attacker, combat, targets, walls, {}, rulesUnderTest);
    check(target.health == 45 && other.health == 100, "No backward or out-of-range hits");
    target.pos = {30, 0};
    walls.addPolygon({{10, -50}, {15, -50}, {15, 50}, {10, 50}});
    common::startAttack(attacker, combat, common::AttackKind::Heavy, {}, rulesUnderTest);
    for (int i = 0; i < 64; ++i)
        common::updateAttack(attacker, combat, targets, walls, {}, rulesUnderTest);
    check(target.health == 45, "Cannot attack through walls");
    target.health = 1;
    common::CollisionWorld open;
    common::startAttack(attacker, combat, common::AttackKind::Light, {}, rulesUnderTest);
    for (int i = 0; i < 16; ++i)
        common::updateAttack(attacker, combat, targets, open, {}, rulesUnderTest);
    check(target.health == 0 && !target.alive, "Lethal damage kills and clamps health");
    common::CombatState victim;
    check(!common::startAttack(target, victim, common::AttackKind::Light, {}, rulesUnderTest),
          "Dead players cannot attack");
    for (unsigned i = 0; i < common::respawnDelayTicks(rulesUnderTest) - 1; ++i)
        check(!common::advanceDeath(target, victim, rulesUnderTest),
              "Death animation must finish before respawn");
    check(common::advanceDeath(target, victim, rulesUnderTest),
          "Respawn must become ready after 2.5 seconds");
    auto name = target.name;
    common::respawn(target, victim, {-140, 620});
    check(target.health == 100 && target.alive && target.pos == sf::Vector2f(-140, 620) &&
              target.name == name,
          "Respawn restores health, position and identity");
    check(common::startAttack(target, victim, common::AttackKind::Heavy, {}, rulesUnderTest),
          "Respawned player can attack");
    target.health = -4;
    common::advanceDeath(target, victim, rulesUnderTest);
    check(!target.alive && target.health == 0, "Negative health also dies");
    common::PlayerState network;
    network.combatDebug = {common::AttackKind::Heavy, 24, {1.f, 0.f}, true, 3};
    sf::Packet packet;
    common::writePlayerState(packet, network);
    common::PlayerState decoded;
    check(common::readPlayerState(packet, decoded), "Combat debug snapshot decoding");
    check(decoded.combatDebug.attack == common::AttackKind::Heavy && decoded.combatDebug.elapsedTicks == 24 &&
              decoded.combatDebug.direction == sf::Vector2f(1, 0) && decoded.combatDebug.hit &&
              decoded.combatDebug.target == 3,
          "Combat debug fields must round-trip accurately");
    check(combat.hitTarget == 1, "Confirmed hit identifies the selected target");
    common::PlayerState mouseAttacker, mouseTarget;
    mouseAttacker.connected = mouseTarget.connected = true;
    mouseAttacker.pos = {0, 0};
    mouseTarget.pos = {30, 0};
    common::CombatState mouseCombat;
    mouseCombat.facing = {-1, 0};
    check(common::startAttack(mouseAttacker, mouseCombat, common::AttackKind::Light, {10, 0}, rulesUnderTest),
          "Mouse-aimed attack starts");
    check(mouseCombat.attackDirection == sf::Vector2f(1, 0),
          "Mouse aim overrides movement direction and normalizes");
    std::vector<common::PlayerState*> mouseTargets{&mouseAttacker, &mouseTarget};
    mouseCombat.facing = {0, 1};
    for (int i = 0; i < 16; ++i)
        common::updateAttack(mouseAttacker, mouseCombat, mouseTargets, open, {}, rulesUnderTest);
    check(mouseTarget.health == 80, "Hit follows captured mouse direction despite movement");
    common::CombatState invalid;
    check(!common::startAttack(mouseAttacker, invalid, common::AttackKind::Heavy,
                               {std::numeric_limits<float>::quiet_NaN(), 0}, rulesUnderTest),
          "Reject invalid aim");
    invalid.facing = {0, 1};
    check(common::startAttack(mouseAttacker, invalid, common::AttackKind::Heavy, {}, rulesUnderTest),
          "Cursor at feet has facing fallback");
    check(invalid.attackDirection == sf::Vector2f(0, 1), "Zero-length aim fallback");
    common::InputCommand input;
    input.aim = {-0.6f, 0.8f};
    input.heavyPressed = true;
    sf::Packet inputPacket;
    common::writeInputCmd(inputPacket, 7, input);
    std::string type;
    inputPacket >> type;
    common::InputCommand received;
    common::PlayerId id;
    check(common::readInputCmd(inputPacket, id, received) && id == 7 && received.aim == input.aim &&
              received.heavyPressed,
          "Mouse aim must round-trip through network input");
    common::PlayerState buffered;
    buffered.connected = true;
    common::CombatState bufferCombat;
    common::startAttack(buffered, bufferCombat, common::AttackKind::Light, {1, 0}, rulesUnderTest);
    std::vector<common::PlayerState*> noTargets;
    for (int i = 0; i < 55; ++i)
        common::updateAttack(buffered, bufferCombat, noTargets, open, {}, rulesUnderTest);
    check(common::requestAttack(buffered, bufferCombat, {1, common::AttackKind::Heavy, {0, 1}, 0},
                                rulesUnderTest),
          "Late click is buffered");
    for (int i = 0; i < 9; ++i)
        common::updateAttack(buffered, bufferCombat, noTargets, open, {}, rulesUnderTest);
    check(buffered.attackSequence == 2 && bufferCombat.attack == common::AttackKind::Heavy &&
              bufferCombat.attackDirection == sf::Vector2f(0, 1),
          "Buffered attack starts at recovery end with original aim");
    common::requestAttack(buffered, bufferCombat, {2, common::AttackKind::Light, {1, 0}, 0}, rulesUnderTest);
    for (int i = 0; i < 64; ++i)
        common::updateAttack(buffered, bufferCombat, noTargets, open, {}, rulesUnderTest);
    check(buffered.attackSequence == 2 && bufferCombat.bufferedAttack == common::AttackKind::None,
          "Early clicks expire rather than replay later");
    check(!common::requestAttack(buffered, bufferCombat, {3, common::AttackKind::Light, {1, 0}, 250},
                                 rulesUnderTest),
          "Expired network input cannot start a surprise attack");
    common::AttackOutbox outbox;
    outbox.enqueue(common::AttackKind::Light, {0, 1}, 100);
    auto first = outbox.requests(100), retry = outbox.requests(150);
    check(first.size() == 1 && retry.size() == 1 && first[0].sequence == retry[0].sequence &&
              retry[0].ageMs == 50,
          "Lost input is retried with the same ID, aim and original age");
    common::AttackInbox inbox;
    check(inbox.accept(retry[0].sequence), "Retried input accepted once");
    check(!inbox.accept(retry[0].sequence), "Lost ACK retry cannot duplicate attack or refresh buffer");
    outbox.enqueue(common::AttackKind::Heavy, {1, 0}, 160);
    auto requests = outbox.requests(170);
    outbox.acknowledge(requests[1].sequence);
    check(outbox.requests(180).size() == 1, "Out-of-order ACK only removes its own request");
    outbox.acknowledge(first[0].sequence);
    check(outbox.requests(190).empty(), "ACK stops retries");
    common::AttackInbox wrapping;
    check(wrapping.accept(0xffffffffu) && wrapping.accept(0) && !wrapping.accept(0xffffffffu),
          "Attack IDs support wrap and reject stale delivery");
    auto attackPacket = common::attackPacket(9, retry[0]);
    attackPacket >> type;
    common::AttackRequest decodedRequest;
    check(common::readAttackRequest(attackPacket, id, decodedRequest) && id == 9 &&
              decodedRequest.ageMs == 50 && decodedRequest.aim == sf::Vector2f(0, 1),
          "Reliable attack packet round trip");
    check(common::inAttackArc({30, 29}, {1, 0}, 70, common::AttackKind::Light, rulesUnderTest),
          "Target just inside narrowed cone");
    check(!common::inAttackArc({30, 31}, {1, 0}, 70, common::AttackKind::Light, rulesUnderTest),
          "Target outside 90-degree cone misses");
    check(target.damageEvents.back().amount == 1 && target.damageEvents.back().source == 0,
          "Overkill event records actual damage and source");
    sf::Packet damagePacket;
    common::writePlayerState(damagePacket, target);
    common::PlayerState damageDecoded;
    check(common::readPlayerState(damagePacket, damageDecoded) &&
              damageDecoded.damageEvents.back().amount == 1 &&
              damageDecoded.damageEvents.back().contact == target.damageEvents.back().contact,
          "Damage event contact and amount survive serialization");
    check((100 + common::attackDescription(common::AttackKind::Light, rulesUnderTest).damageMax - 1) /
                  common::attackDescription(common::AttackKind::Light, rulesUnderTest).damageMax ==
              5,
          "Five lights should defeat a full-health player");
    check((100 + common::attackDescription(common::AttackKind::Heavy, rulesUnderTest).damageMax - 1) /
                  common::attackDescription(common::AttackKind::Heavy, rulesUnderTest).damageMax ==
              3,
          "Three heavys should defeat a full-health player");
    {
        rulesUnderTest.spell.damageMin = rulesUnderTest.spell.damageMax = 30;
        common::PlayerState caster, a, b, friendPlayer, outside;
        caster.connected = a.connected = b.connected = friendPlayer.connected = outside.connected = true;
        caster.pos = {0, 0};
        caster.team = friendPlayer.team = 0;
        a.team = b.team = outside.team = 1;
        a.pos = {200, 0};
        b.pos = {300, 0};
        friendPlayer.pos = {210, 0};
        outside.pos = {321, 0};
        common::CombatState cast;
        common::CollisionWorld empty;
        std::vector<common::PlayerState*> targets{&caster, &a, &b, &friendPlayer, &outside};
        check(!common::startAttack(caster, cast, common::AttackKind::Explosion, {501, 0}, rulesUnderTest),
              "Spell range must be enforced");
        check(common::startAttack(caster, cast, common::AttackKind::Explosion, {200, 0}, rulesUnderTest),
              "Spell did not start");
        for (int i = 0; i < 23; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(a.health == 100, "Spell hit before windup");
        common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(a.health == 70 && b.health == 70 && friendPlayer.health == 100 && outside.health == 100,
              "AoE radius or teams incorrect");
        check(caster.spellSequence == 1 && caster.spellPosition == sf::Vector2f(200, 0) &&
                  a.damageEvents.back().source == 0,
              "Spell effect or hurt attribution missing");
        check(!common::startAttack(caster, cast, common::AttackKind::Explosion, {200, 0}, rulesUnderTest),
              "Spell cooldown bypassed");
        for (int i = 24; i < 129; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(cast.attack == common::AttackKind::None && a.health == 70,
              "Spell repeated or cooldown never ended");
        caster.pos = {200, 0};
        check(common::startAttack(caster, cast, common::AttackKind::Explosion, {200, 0}, rulesUnderTest),
              "Self-area cast failed");
        for (int i = 0; i < 24; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(caster.health == 70 && a.health == 40 && friendPlayer.health == 100,
              "Spell must hurt caster and enemies but spare teammates");
        check(caster.damageEvents.back().source == 0, "Self damage attribution lost");
        cast = {};
        rulesUnderTest.friendlyFire = true;
        common::startAttack(caster, cast, common::AttackKind::Explosion, {200, 0}, rulesUnderTest);
        for (int i = 0; i < 24; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(caster.health == 40 && friendPlayer.health == 70,
              "Friendly fire should allow teammate damage without disabling self damage");
        rulesUnderTest.friendlyFire = false;
        std::vector<common::PlayerState> states(common::MAX_PLAYERS);
        states[0] = caster;
        states[0].facing = {0.f, -1.f};
        sf::Packet packet;
        common::writeWorldPacket(packet, states);
        std::string type;
        packet >> type;
        std::vector<common::PlayerState> result;
        check(common::readWorldPacket(packet, result) && result[0].spellSequence == 3 &&
                  result[0].spellPosition == caster.spellPosition,
              "Spell snapshot did not roundtrip");
        check(result[0].facing == sf::Vector2f(0.f, -1.f),
              "Visual facing did not roundtrip independently of attack direction");
    }
    rulesUnderTest = unstunnedRules();
    {
        common::PlayerState caster, victim, ally;
        caster.connected = victim.connected = ally.connected = true;
        caster.pos = {0, 0};
        victim.pos = ally.pos = {200, 0};
        caster.team = ally.team = 0;
        victim.team = 1;
        common::CombatState cast;
        common::CollisionWorld empty;
        std::vector<common::PlayerState*> targets{&caster, &victim, &ally};
        check(common::requestAttack(caster, cast, {0, common::AttackKind::Lightning, {200, 0}, 0},
                                    rulesUnderTest),
              "Lightning request rejected");
        for (int i = 0; i < 15; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(victim.health == 100, "Lightning hit before windup");
        common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(victim.health >= 96 && victim.health <= 99 && caster.spellSequence == 1,
              "First lightning tick missing");
        for (int i = 0; i < 21; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(caster.spellSequence == 1, "Lightning hit before one third second");
        common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(caster.spellSequence == 2, "Lightning repeat missing");
        for (int i = 38; i < 272; ++i)
            common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
        check(caster.spellSequence == 9 && cast.attack == common::AttackKind::None,
              "Lightning duration/cooldown incorrect");
        check(ally.health == 100 && victim.health >= 64 && victim.health <= 91,
              "Lightning damage bounds or team filter wrong");
        for (const auto& hit : victim.damageEvents)
            check(hit.amount >= 1 && hit.amount <= 4, "Lightning damage outside configured range");
        check(caster.spellEffect == common::AttackKind::Lightning, "Lightning visual not tagged");
        rulesUnderTest.spell.effectRadius = 25;
        rulesUnderTest.spell.castRange = 250;
        rulesUnderTest.spell.damageMin = 5;
        rulesUnderTest.spell.damageMax = 30;
        caster.pos = {0, 0};
        victim.pos = {200, 0};
        for (int trial = 0; trial < 32; ++trial) {
            victim.health = 100;
            victim.alive = true;
            cast = {};
            common::startAttack(caster, cast, common::AttackKind::Explosion, {200, 0}, rulesUnderTest);
            for (int i = 0; i < 24; ++i)
                common::updateAttack(caster, cast, targets, empty, {}, rulesUnderTest);
            check(victim.health >= 70 && victim.health <= 95, "Explosion damage outside configured range");
        }
    }
    rulesUnderTest = unstunnedRules();
    {
        common::PlayerState p;
        p.connected = true;
        p.pos = {0, 0};
        common::CombatState c;
        common::CollisionWorld walls;
        common::updateAim(p, c, {0, 100}, walls, rulesUnderTest);
        check(c.facing == sf::Vector2f(0, 1), "Idle facing must follow cursor");
        common::startAttack(p, c, common::AttackKind::Light, {1, 0}, rulesUnderTest);
        common::updateAim(p, c, {0, 100}, walls, rulesUnderTest);
        check(c.attackDirection == sf::Vector2f(0, 1), "Windup aim frozen too early");
        c.elapsedTicks = common::attackDescription(c.attack, rulesUnderTest).startupTicks;
        common::updateAim(p, c, {-100, 0}, walls, rulesUnderTest);
        check(c.attackDirection == sf::Vector2f(0, 1), "Active swing aim must lock");
        for (auto kind : {common::AttackKind::Explosion, common::AttackKind::Lightning}) {
            c = {};
            common::startAttack(p, c, kind, {100, 0}, rulesUnderTest);
            common::updateAim(p, c, {0, 100}, walls, rulesUnderTest);
            check(c.spellTarget == sf::Vector2f(100, 0) && p.spellPosition == sf::Vector2f(100, 0),
                  "Spell windup must retain initial click");
            check(c.facing == sf::Vector2f(1, 0), "Spell windup must face the fixed target");
            p.pos = {100, -100};
            common::updateAim(p, c, {-200, 0}, walls, rulesUnderTest);
            check(c.facing == sf::Vector2f(0, 1), "Moving caster must keep facing spell target");
            p.pos = {0, 0};
            c.elapsedTicks = common::attackDescription(c.attack, rulesUnderTest).startupTicks;
            common::updateAim(p, c, {0, 200}, walls, rulesUnderTest);
            check(c.spellTarget == sf::Vector2f(100, 0), "Active spell target must stay locked");
            check(c.facing == sf::Vector2f(0, 1), "Cursor facing must resume after spell windup");
        }
        walls.addPolygon({{40, -20}, {60, -20}, {60, 20}, {40, 20}});
        check(!common::spellTargetValid(p, common::AttackKind::Explosion, {100, 0}, walls, rulesUnderTest),
              "Wall-blocked cast accepted");
        common::InputCommand cmd;
        cmd.hasCursor = true;
        cmd.movementFacing = true;
        cmd.cursor = {42, 100};
        sf::Packet wire;
        common::writeInputCmd(wire, 0, cmd);
        std::string type;
        wire >> type;
        common::PlayerId id;
        common::InputCommand received;
        check(common::readInputCmd(wire, id, received) && received.hasCursor && received.movementFacing &&
                  received.cursor == cmd.cursor,
              "Cursor must survive network transport");
    }
    {
        common::ServerSettings rules;
        rulesUnderTest = rules;
        common::PlayerState p;
        p.connected = true;
        common::CombatState c;
        common::CollisionWorld empty;
        common::applyDamage(p, 2, -1, p.pos, rulesUnderTest);
        check(p.stunTicks == 32, "Default damage stun must last half a second");
        for (int i = 0; i < 10; ++i)
            common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
        common::applyDamage(p, 2, -1, p.pos, rulesUnderTest);
        check(p.stunTicks == 32, "Repeated damage must reset stun");
        for (int i = 0; i < 32; ++i) {
            check(!common::startAttack(p, c, common::AttackKind::Light, {1, 0}, rulesUnderTest),
                  "Attacks must be blocked during stun");
            common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
        }
        check(common::startAttack(p, c, common::AttackKind::Light, {1, 0}, rulesUnderTest),
              "Attacks must resume when stun ends");
        rules.damageStunSeconds = .25;
        rulesUnderTest = rules;
        common::applyDamage(p, 1, -1, p.pos, rulesUnderTest);
        check(p.stunTicks == 16, "Custom damage stun ignored");
        sf::Packet config;
        common::writeSettings(config, rules);
        common::ServerSettings received;
        check(common::readSettings(config, received) && received.damageStunSeconds == .25,
              "Stun setting failed network roundtrip");
        std::vector<common::PlayerState> states(common::MAX_PLAYERS);
        states[0] = p;
        sf::Packet world;
        common::writeWorldPacket(world, states);
        std::string type;
        world >> type;
        std::vector<common::PlayerState> snapshot;
        check(common::readWorldPacket(world, snapshot) && snapshot[0].stunTicks == 16,
              "Stun countdown failed network roundtrip");
        common::respawn(p, c, {0, 0});
        check(p.stunTicks == 0, "Respawn must clear stun");
        rulesUnderTest = unstunnedRules();
    }
    {
        auto rules = unstunnedRules();
        rules.respawnSeconds = .25;
        rulesUnderTest = rules;
        common::PlayerState p;
        p.alive = false;
        p.health = 0;
        common::CombatState c;
        for (int i = 0; i < 15; ++i)
            check(!common::advanceDeath(p, c, rulesUnderTest), "Custom respawn fired too early");
        check(common::advanceDeath(p, c, rulesUnderTest), "Custom respawn delay ignored");
        sf::Packet wire;
        common::writeSettings(wire, rules);
        common::ServerSettings decoded;
        check(common::readSettings(wire, decoded) && decoded.respawnSeconds == .25,
              "Respawn setting not synchronized");
        rulesUnderTest = unstunnedRules();
    }
    {
        rulesUnderTest = unstunnedRules();
        common::PlayerState p;
        p.connected = true;
        p.pos = {0, 0};
        common::CombatState c;
        common::CollisionWorld empty;
        check(common::startAttack(p, c, common::AttackKind::Explosion, {200, 0}, rulesUnderTest),
              "Explosion starts");
        for (int i = 0; i < 25; ++i)
            common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
        check(c.attack == common::AttackKind::None && p.explosionCooldown > 0,
              "Explosion must enter its own cooldown");
        check(!common::startAttack(p, c, common::AttackKind::Explosion, {200, 0}, rulesUnderTest),
              "Explosion cooldown must block explosion");
        check(common::startAttack(p, c, common::AttackKind::Lightning, {200, 0}, rulesUnderTest),
              "Explosion cooldown must allow lightning");
        const auto before = p.explosionCooldown;
        common::updateAttack(p, c, {&p}, empty, {}, rulesUnderTest);
        check(p.explosionCooldown == before - 1, "Explosion cooldown must tick during lightning");
        std::vector<common::PlayerState> states(common::MAX_PLAYERS);
        states[0] = p;
        states[0].pingMs = 42;
        sf::Packet wire;
        common::writeWorldPacket(wire, states);
        std::string type;
        wire >> type;
        std::vector<common::PlayerState> received;
        check(common::readWorldPacket(wire, received) && received[0].pingMs == 42 &&
                  received[0].explosionCooldown == p.explosionCooldown,
              "Cooldown and ping snapshot roundtrip");
    }
    std::cout << "PASS: light/heavy damage, windup, cooldown, range, facing, walls, death and respawn\n";
}
