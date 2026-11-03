#include "server/game_simulation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() try {
    std::ostringstream output;
    common::Logger logger(output);
    common::ServerSettings settings;
    settings.map = "city";
    settings.slots = 3;
    settings.bots = 0;
    settings.botAI = false;
    settings.respawnSeconds = common::TICK_DT;
    GameSimulation game(settings, logger);
    check(game.join(0, "Visitor", 1), "Join failed");
    const auto buttons = game.triggers.characterSelect().buttons();
    check(buttons.size() == 11, "Missing city appearance/class selection buttons");
    auto& player = game.players[0].state;
    player.health = 63;
    for (const auto& button : buttons) {
        if (button.changesClass)
            continue;
        player.pos = button.center;
        game.step();
        check(game.snapshot()[0].character == button.value, "Server did not apply button selection");
        check(player.health == 63 && player.name == "Visitor" && player.team == 0,
              "Selection changed health, name, or team");
    }
    for (const auto& button : buttons) {
        if (!button.changesClass)
            continue;
        player.pos = button.center;
        player.explosionCooldown = 80;
        player.lightningCooldown = 100;
        game.step();
        check(int(game.snapshot()[0].playerClass) == button.value && player.character == 6,
              "Class button changed appearance or selected the wrong class");
        check(player.health == 63 && player.explosionCooldown == 79 && player.lightningCooldown == 99,
              "Class selection reset health or spell cooldowns");
        sf::Packet packet;
        common::writePlayerState(packet, player);
        common::PlayerState replica;
        check(common::readPlayerState(packet, replica) && replica.playerClass == player.playerClass,
              "Selected class did not replicate");
    }
    for (const auto& button : buttons)
        if (button.changesClass && button.value == int(common::PlayerClass::Charger))
            player.pos = button.center;
    game.step();
    common::InputCommand moving;
    moving.move = {1, 0};
    moving.sprint = true;
    game.input(0, moving);
    const auto oldPosition = player.pos;
    game.step();
    check(std::abs((player.pos - oldPosition).length() - common::RUN_SPEED * 1.25f * common::TICK_DT) < .01f,
          "Charger class speed bonus was not applied by the server");
    game.input(0, {});
    auto& bot = game.players[1].state;
    bot.connected = true;
    bot.team = 0;
    bot.character = 2;
    bot.pos = buttons[5].center;
    game.step();
    check(bot.character == 2, "Passing bot lost its role appearance on a selection button");
    player.health = 0;
    player.alive = false;
    game.step();
    check(player.alive && player.character == 6, "Respawn discarded selected appearance");
    check(player.playerClass == common::PlayerClass::Charger, "Respawn discarded selected class");
    game.changeTeam(0, 2);
    check(player.character == 6, "Team change discarded selected appearance");
    check(player.playerClass == common::PlayerClass::Charger, "Team change discarded selected class");
    game.restartRound();
    check(game.players[0].state.character == 6, "Round restart discarded selected appearance");
    check(game.players[0].state.playerClass == common::PlayerClass::Charger,
          "Round restart discarded selected class");
    GameSimulation replacement(settings, logger, nullptr, false);
    replacement.restoreRoster(game);
    check(replacement.players[0].state.character == 6, "Map reload discarded selected appearance");
    check(replacement.players[0].state.playerClass == common::PlayerClass::Charger,
          "Map reload discarded selected class");

    // Validate actual combat damage, not just the class descriptions.
    auto damage = [](common::PlayerClass value, common::AttackKind attack) {
        common::ServerSettings rules;
        rules.lightDamageMin = rules.lightDamageMax = 20;
        rules.spell.damageMin = rules.spell.damageMax = 20;
        common::PlayerState attacker, victim;
        attacker.connected = victim.connected = true;
        attacker.team = 0;
        victim.team = 1;
        attacker.pos = {0, 0};
        victim.pos = common::isSpell(attack) ? sf::Vector2f{300, 0} : sf::Vector2f{40, 0};
        attacker.playerClass = value;
        common::CombatState combat;
        common::CollisionWorld walls;
        std::vector<common::PlayerState*> targets{&attacker, &victim};
        check(common::startAttack(attacker, combat, attack, victim.pos, rules), "Class combat start failed");
        for (int tick = 0; tick < common::TICK_RATE; ++tick)
            common::updateAttack(attacker, combat, targets, walls, {}, rules);
        return 100 - victim.health;
    };
    check(damage(common::PlayerClass::Bruiser, common::AttackKind::Light) == 25,
          "Bruiser melee bonus missing");
    check(damage(common::PlayerClass::Caster, common::AttackKind::Explosion) == 25,
          "Caster spell bonus missing");
    check(damage(common::PlayerClass::Adventurer, common::AttackKind::Light) == 20 &&
              damage(common::PlayerClass::Caster, common::AttackKind::Light) == 20 &&
              damage(common::PlayerClass::Bruiser, common::AttackKind::Explosion) == 20,
          "Class bonus applied to the wrong attack type");

    const auto fixture = std::filesystem::temp_directory_path() / "marpg-invalid-character-select.tmx";
    auto registry = std::make_shared<common::TriggerRegions>();
    common::CharacterSelect select(registry);
    for (const auto* property : {"<property name=\"character\" type=\"int\" value=\"7\"/>",
                                 "<property name=\"character\" type=\"int\" value=\"-1\"/>",
                                 "<property name=\"character\" value=\"Player\"/>", ""}) {
        std::ofstream(fixture) << "<map version=\"1.10\" orientation=\"isometric\" width=\"2\" height=\"2\" "
                                  "tilewidth=\"128\" tileheight=\"64\"><objectgroup name=\"Triggers\">"
                                  "<object name=\"Invalid\" type=\"CharacterSelect\" x=\"0\" y=\"0\" "
                                  "width=\"32\" height=\"32\"><properties>"
                               << property << "</properties></object></objectgroup></map>";
        tmx::Map map;
        check(map.load(fixture.string()), "Invalid-selection fixture did not load");
        bool rejected = false;
        try {
            select.load(map);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected, "Invalid character selection property was accepted");
    }
    std::filesystem::remove(fixture);
    // Malformed classes must not partially apply an otherwise valid player snapshot.
    for (const auto value : {std::uint8_t(4), std::uint8_t(255)}) {
        common::PlayerState invalid;
        invalid.playerClass = static_cast<common::PlayerClass>(value);
        sf::Packet packet;
        common::writePlayerState(packet, invalid);
        common::PlayerState destination;
        destination.name = "Keep me";
        check(!common::readPlayerState(packet, destination) && destination.name == "Keep me",
              "Malformed class snapshot was accepted or partially applied");
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
