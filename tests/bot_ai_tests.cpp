#include "common/bot_ai.hpp"
#include <iostream>
#include <set>
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char** argv) try {
    check(argc == 2, "Map path required");
    common::TriggerSystem hazards;
    hazards.load(argv[1]);
    common::CollisionWorld walls;
    walls.addPolygon({{50, 500}, {150, 500}, {150, 800}, {50, 800}});
    common::Navigation nav(argv[1], walls, hazards);
    sf::Vector2f start{-100, 650}, goal{300, 650};
    check(!nav.clear(start, goal), "Test wall must block direct travel");
    auto path = nav.path(start, goal);
    check(!path.empty(), "A* must find a route around the wall");
    auto previous = start;
    for (auto point : path) {
        check(nav.clear(previous, point), "Path crossed a wall or hazard");
        previous = point;
    }
    check(nav.path(start, {128, -100}).empty(), "AI must not path off the floor");
    common::PlayerState bot, enemy, ally;
    bot.connected = enemy.connected = ally.connected = true;
    bot.team = ally.team = 0;
    enemy.team = 1;
    bot.pos = start;
    enemy.pos = goal;
    enemy.health = 10000;
    bot.health = 10000;
    ally.pos = {-250, 650};
    common::CombatState combat;
    common::BotAI ai(nav, walls, 123);
    std::vector<common::PlayerState*> players{&bot, &enemy, &ally};
    std::set<common::AttackKind> attacks;
    for (int tick = 0; tick < 90 * common::TICK_RATE; ++tick) {
        if (tick == 128)
            enemy.pos = {300, 700}; // Replan for a moving target.
        auto old = bot.pos;
        ai.update(0, bot, combat, players);
        check((bot.pos - old).length() <= common::RUN_SPEED * common::TICK_DT + .01f,
              "AI exceeded player run speed");
        check(nav.safePoint(bot.pos), "AI entered a wall or hazard");
        check(ai.target(0) == 1, "Bot targeted its teammate");
        if (combat.attack != common::AttackKind::None)
            attacks.insert(combat.attack);
        if ((common::isSpell(combat.attack)) && combat.elapsedTicks == 0) {
            check(combat.spellTarget == enemy.pos, "Bot spell did not target enemy world position");
            check(bot.vel.length() == 0, "Bot should stand still while casting");
        }
        common::updateAttack(bot, combat, players, walls);
    }
    check(enemy.health < 10000, "Bot never reached/attacked enemy behind wall");
    check(ally.health == 100, "Bot damaged its teammate");
    check(attacks == std::set<common::AttackKind>{common::AttackKind::Light, common::AttackKind::Heavy},
          "Bruiser must use both melee attacks and no spells");
    enemy.connected = false;
    ally.team = 1;
    ai.update(0, bot, combat, players);
    check(ai.target(0) == 2, "Bot did not replace disconnected target");
    ally.alive = false;
    ai.update(0, bot, combat, players);
    check(ai.target(0) == -1 && bot.vel.length() == 0, "Bot must idle without living enemies");
    enemy.connected = true;
    enemy.alive = true;
    ally.alive = true;
    std::set<int> chosen;
    for (int i = 0; i < 30; ++i) {
        ai.reset(0);
        ai.update(0, bot, combat, players);
        chosen.insert(ai.target(0));
    }
    check(chosen.size() == 2, "Random target selection did not vary");
    bot.alive = false;
    ai.update(0, bot, combat, players);
    check(ai.target(0) == -1, "Death must clear bot target");
    {
        common::BotAI retaliation(nav, walls, 17);
        std::vector<common::PlayerState> group(4);
        for (auto& p : group) {
            p.connected = true;
            p.pos = {-100, 650};
        }
        group[0].team = group[1].team = 0;
        group[2].team = group[3].team = 1;
        group[1].pos = {-250, 650};
        group[2].pos = {300, 650};
        group[3].pos = {300, 700};
        std::vector<common::PlayerState*> refs;
        for (auto& p : group)
            refs.push_back(&p);
        common::CombatState first, second;
        retaliation.update(1, group[1], second, refs);
        const int claimed = retaliation.target(1), available = claimed == 2 ? 3 : 2;
        common::applyDamage(group[0], 1, available, group[0].pos);
        retaliation.update(0, group[0], first, refs);
        check(retaliation.target(0) == available, "Hit bot must retaliate against available attacker");
        common::applyDamage(group[0], 1, claimed, group[0].pos);
        retaliation.update(0, group[0], first, refs);
        check(retaliation.target(0) == available, "Bot must not steal another bot's target on damage");
    }
    {
        common::BotAI roles(nav, walls, 72);
        std::vector<common::PlayerState> group(4);
        group[1].connected = group[3].connected = true;
        group[1].team = 0;
        group[3].team = 1;
        group[1].pos = {-200, 650};
        group[3].pos = {-200, 750};
        group[1].health = group[3].health = 10000;
        std::vector<common::PlayerState*> refs;
        for (auto& p : group)
            refs.push_back(&p);
        common::CombatState casting;
        // With spells cooling down, the caster must create space rather than close
        // for melee.
        group[1].explosionCooldown = group[1].lightningCooldown = group[1].beamCooldown = 100;
        const auto before = (group[1].pos - group[3].pos).length();
        roles.update(1, group[1], casting, refs);
        check((group[1].pos - group[3].pos).length() > before, "Caster did not retreat from melee");
        check(nav.safePoint(group[1].pos), "Caster retreated onto unsafe floor");
        group[1].explosionCooldown = group[1].lightningCooldown = group[1].beamCooldown = 0;
        std::set<common::AttackKind> spells;
        for (int tick = 0; tick < 20 * common::TICK_RATE; ++tick) {
            roles.update(1, group[1], casting, refs);
            if (casting.attack != common::AttackKind::None) {
                spells.insert(casting.attack);
                check(group[1].vel.length() == 0, "Caster moved during casting/recovery");
                check(casting.spellTarget == group[3].pos, "Caster did not lock enemy world position");
            }
            check(nav.safePoint(group[1].pos), "Caster entered a hazard");
            common::updateAttack(group[1], casting, refs, walls);
        }
        check(spells == std::set<common::AttackKind>{common::AttackKind::Explosion,
                                                     common::AttackKind::Lightning, common::AttackKind::Beam},
              "Caster must use all three spells and no melee");
        check(group[3].health < 10000, "Caster never damaged its opponent");
        // Keep a wall between the caster and its target: no spell may start through
        // it.
        roles.reset(1);
        casting = {};
        group[1].pos = start;
        group[3].pos = goal;
        group[1].explosionCooldown = group[1].lightningCooldown = group[1].beamCooldown = 0;
        roles.update(1, group[1], casting, refs);
        check(casting.attack == common::AttackKind::None, "Caster cast through a wall");
    }
    {
        common::BotAI roles(nav, walls, 73);
        std::vector<common::PlayerState> group(4);
        group[2].connected = group[3].connected = true;
        group[2].team = 0;
        group[3].team = 1;
        group[2].pos = {-200, 650};
        group[3].pos = {0, 650};
        std::vector<common::PlayerState*> refs;
        for (auto& p : group)
            refs.push_back(&p);
        common::CombatState charging;
        for (int tick = 0; tick < common::TICK_RATE / 2; ++tick) {
            roles.update(2, group[2], charging, refs);
            check(group[2].pos == sf::Vector2f{-200, 650}, "Charge skipped its half-second windup");
        }
        // Dodge after the windup: the rush must retain the original destination.
        group[3].pos = {0, 760};
        bool rushed = false;
        for (int tick = 0; tick < 20; ++tick) {
            const auto old = group[2].pos;
            roles.update(2, group[2], charging, refs);
            const float speed = (group[2].pos - old).length() / common::TICK_DT;
            rushed |= speed > common::RUN_SPEED;
            check(speed <= common::RUN_SPEED * 1.8f + .01f, "Charge exceeded its speed limit");
            check(group[2].pos.y == 650.f, "Charge homed toward a dodging target");
            check(nav.safePoint(group[2].pos), "Charge crossed unsafe geometry");
        }
        check(rushed, "Charger never rushed after its windup");
        // Damage during windup cancels the rush; normal chasing remains allowed.
        roles.reset(2);
        group[2].pos = {-200, 650};
        group[3].pos = {0, 650};
        roles.update(2, group[2], charging, refs);
        group[0].connected = true;
        group[0].team = 1;
        group[0].pos = {0, 760};
        common::applyDamage(group[2], 1, 0, group[2].pos);
        for (int tick = 0; tick < common::TICK_RATE; ++tick) {
            roles.update(2, group[2], charging, refs);
            check(group[2].vel.length() <= common::RUN_SPEED + .01f, "Interrupted charger still rushed");
            common::updateAttack(group[2], charging, refs, walls);
        }
        check(roles.target(2) == 0, "Charger did not retaliate against new attacker");
        group[0].connected = false;
        // When the enemy stays put, arriving at the charge destination must lead to a heavy hit.
        roles.reset(2);
        charging = {};
        group[2].stunTicks = 0;
        group[2].pos = {-200, 650};
        group[3].pos = {0, 650};
        bool heavy = false;
        const auto healthBefore = group[3].health;
        for (int tick = 0; tick < 2 * common::TICK_RATE; ++tick) {
            roles.update(2, group[2], charging, refs);
            heavy |= charging.attack == common::AttackKind::Heavy;
            check(!common::isSpell(charging.attack), "Charger cast a spell");
            common::updateAttack(group[2], charging, refs, walls);
        }
        check(heavy && group[3].health < healthBefore, "Charge arrival did not produce a heavy hit");
        // A new charge cannot be prepared through the fixture wall.
        roles.reset(2);
        charging = {};
        group[2].stunTicks = 0;
        group[2].pos = {0, 650};
        group[3].pos = {200, 650};
        for (int tick = 0; tick < 45; ++tick) {
            const auto old = group[2].pos;
            roles.update(2, group[2], charging, refs);
            check(nav.clear(old, group[2].pos), "Charger crossed a wall");
        }
    }
    std::cout << "PASS: bruiser melee, caster spells/retreat, charge "
                 "windup/dodge/interruption, "
                 "A* wall detours, safe floor, moving targets, teammate filtering, "
                 "retargeting and death\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
