#include "client/kill_feed.hpp"
#include "client/spell_effects.hpp"
#include "common/combat_system.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main(int argc, char** argv) try {
    check(argc == 2, "Repository path required");
    common::ServerSettings rules;
    rules.beam.damageMin = rules.beam.damageMax = 40;
    rules.beam.windupSeconds = .25;
    rules.beam.castRange = 600;
    common::CollisionWorld walls;
    walls.addPolygon({{400, -100}, {420, -100}, {420, 100}, {400, 100}});
    std::vector<common::PlayerState> players(7);
    std::vector<common::PlayerState*> refs;
    for (auto& p : players) {
        p.connected = true;
        p.team = 1;
        refs.push_back(&p);
    }
    auto& caster = players[0];
    caster.team = 0;
    caster.pos = {0, 0};
    players[1].pos = {100, 0};
    players[2].pos = {250, 20}; // Inside beam half-width plus the target's foot radius.
    players[3].pos = {250, 30}; // Outside the beam.
    players[4].pos = {500, 0};  // Behind the wall.
    players[5].pos = {-50, 0};  // Behind the caster.
    players[6].pos = {200, 0};
    players[6].team = 0;
    common::CombatState combat;
    check(common::spellTargetValid(caster, common::AttackKind::Beam, {1000, 0}, walls, rules),
          "Directional beam aim should accept a cursor beyond its range or a wall");
    check(!common::spellTargetValid(caster, common::AttackKind::Beam, caster.pos, walls, rules),
          "Zero-length beam aim was accepted");
    check(common::startAttack(caster, combat, common::AttackKind::Beam, {1000, 0}, rules),
          "Beam cast failed");
    for (unsigned tick = 1; tick < common::attackDescription(common::AttackKind::Beam, rules).startupTicks;
         ++tick) {
        common::updateAttack(caster, combat, refs, walls, {}, rules);
        check(players[1].health == 100, "Beam hit before its windup completed");
    }
    common::updateAttack(caster, combat, refs, walls, {}, rules);
    check(players[1].health == 60 && players[2].health == 60, "Beam did not pierce both aligned targets");
    check(players[3].health == 100 && players[4].health == 100 && players[5].health == 100 &&
              players[6].health == 100 && caster.health == 100,
          "Beam hit outside its line, through a wall, behind its caster, or damaged an ally/self");
    check(std::abs(caster.spellPosition.x - 400) < .05f && caster.spellPosition.y == 0 &&
              caster.spellOrigin == caster.pos && caster.spellEffect == common::AttackKind::Beam,
          "Authoritative beam endpoints or event kind are wrong");
    common::updateAttack(caster, combat, refs, walls, {}, rules); // Complete the one-tick active window.
    check(caster.beamCooldown > 0 && caster.explosionCooldown == 0 && caster.lightningCooldown == 0,
          "Beam did not use its independent cooldown");
    check(!common::startAttack(caster, combat, common::AttackKind::Beam, {300, 0}, rules),
          "Beam bypassed its cooldown");
    for (int tick = 0; tick < 20; ++tick)
        common::updateAttack(caster, combat, refs, walls, {}, rules);
    check(players[1].health == 60, "Beam damaged a target more than once");
    caster.playerClass = common::PlayerClass::Caster;
    check(!common::startAttack(caster, combat, common::AttackKind::Beam, {300, 0}, rules),
          "Changing class bypassed the beam cooldown");
    common::respawn(caster, combat, {0, 0});
    check(caster.beamCooldown == 0, "Respawn did not reset beam cooldown");
    check(common::startAttack(caster, combat, common::AttackKind::Beam, {300, 0}, rules),
          "Caster beam failed");
    for (int tick = 0; tick < common::TICK_RATE; ++tick)
        common::updateAttack(caster, combat, refs, walls, {}, rules);
    check(players[1].health == 10, "Caster class bonus did not apply to beam damage");
    // Damage during windup interrupts the beam without emitting a visual or hit.
    common::respawn(caster, combat, {0, 0});
    const auto event = caster.spellSequence;
    check(common::startAttack(caster, combat, common::AttackKind::Beam, {300, 0}, rules),
          "Interrupt cast failed");
    common::applyDamage(caster, 1, 1, caster.pos);
    common::updateAttack(caster, combat, refs, walls, {}, rules);
    check(combat.attack == common::AttackKind::None && caster.spellSequence == event &&
              players[1].health == 10,
          "Interrupted beam still fired");
    common::respawn(caster, combat, {0, 0});
    caster.playerClass = common::PlayerClass::Adventurer;
    rules.friendlyFire = true;
    check(common::startAttack(caster, combat, common::AttackKind::Beam, {300, 0}, rules), "Friendly beam cast failed");
    for (int tick = 0; tick < common::TICK_RATE; ++tick)
        common::updateAttack(caster, combat, refs, walls, {}, rules);
    check(players[6].health == 60 && caster.health == 100, "Beam ignored friendly fire setting or hit its caster");
    // Range cap and angled-wall clipping retain a straight line.
    common::CollisionWorld empty;
    check(common::beamEndpoint({0, 0}, {50, 0}, empty, 600) == sf::Vector2f{600, 0},
          "Beam ignored its full range");
    check(!common::beamContains({0, 0}, {600, 0}, {601, 0}, 12), "Beam hit beyond its range");
    common::CollisionWorld angled;
    angled.addPolygon({{200, -100}, {300, 0}, {200, 100}});
    auto end = common::beamEndpoint({0, 0}, {600, 100}, angled, 600);
    check(end.x < 300 && std::abs(end.y - end.x / 6.f) < .01f, "Beam slid along an angled wall");
    // Replicate beam events/cooldown and negotiated server settings.
    caster.spellOrigin = {12, 34};
    caster.beamCooldown = 91;
    sf::Packet packet;
    common::writePlayerState(packet, caster);
    common::PlayerState replica;
    check(common::readPlayerState(packet, replica) && replica.spellOrigin == caster.spellOrigin &&
              replica.spellPosition == caster.spellPosition && replica.beamCooldown == 91,
          "Beam state did not replicate");
    sf::Packet settings;
    common::writeSettings(settings, rules);
    common::ServerSettings restored;
    check(common::readSettings(settings, restored) && restored.beam.castRange == 600 &&
              restored.beam.damageMin == 40,
          "Beam server settings did not replicate");
    check(common::KeyBindings{}[common::Action::Beam] == std::vector<int>{common::parseBinding("R")},
          "Beam default binding is not R");
    check(std::string(KillFeed::cause(common::KillCause::Beam)) == "BEAM", "Beam kill label missing");
    const auto root = std::filesystem::path(argv[1]);
    check(common::loadServerSettings((root / "server.toml").string()).beam.castRange == 975,
          "Beam TOML settings did not load");
    check(common::loadClientSettings((root / "client.toml").string()).bindings[common::Action::Beam] ==
              std::vector<int>{common::parseBinding("R")}, "Beam TOML binding did not load");
    SpellEffects effects(root / "assets/sprites/Free Pixel Art Explosions/PNG/Explosion", rules);
    std::vector<common::PlayerState> visual(1);
    visual[0].connected = true;
    effects.observe(visual, 0);
    visual[0].spellSequence = 1;
    visual[0].spellEffect = common::AttackKind::Beam;
    visual[0].spellOrigin = {60, 200};
    visual[0].spellPosition = {650, 200};
    effects.observe(visual, 0);
    sf::RenderTexture preview({800, 360});
    preview.clear(sf::Color(20, 25, 35));
    effects.draw(preview);
    preview.display();
    auto rendered = preview.getTexture().copyToImage();
    check(rendered.getPixel({300, 168}) != sf::Color(20, 25, 35) &&
              rendered.getPixel({700, 168}) == sf::Color(20, 25, 35),
          "Beam visual has incorrect endpoints");
    check(rendered.saveToFile(std::filesystem::temp_directory_path() / "marpg-beam-preview.png"),
          "Beam preview failed");
    effects.observe(visual, .4f);
    preview.clear(sf::Color(20, 25, 35));
    effects.draw(preview);
    preview.display();
    check(preview.getTexture().copyToImage().getPixel({300, 168}) == sf::Color(20, 25, 35),
          "Beam effect did not expire");
    std::cout << "PASS: beam piercing, walls, width/range, friendly fire, windup, cooldown, class bonus, "
                 "interruption, replication and rendering\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
