#include "server/game_simulation.hpp"
#include <sstream>
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    std::ostringstream output;
    common::Logger logger(output);
    common::ServerSettings settings;
    settings.map = "arena";
    settings.slots = 4;
    settings.bots = 0;
    settings.botAI = false;
    settings.teams = 2;
    settings.honorTeamRequests = false;
    GameSimulation game(settings, logger);
    check(game.join(0, "Alice", 1) && game.join(1, "Bob", 2), "Join failed");
    const auto original = game.players[0].state.team;
    game.changeTeam(0, 2);
    check(game.players[0].state.team == original, "Team request ignored balance policy");
    game.settings.honorTeamRequests = true;
    game.players[0].state.character = 6;
    game.changeTeam(0, 2);
    check(game.players[0].state.character == 6, "Team change discarded selected appearance");
    check(game.players[0].state.team == 1 && game.players[0].state.teleportSequence == 1,
          "Forced team change did not respawn");
    game.setBots(2);
    check(game.botMask() == 12, "Bots did not occupy free slots");
    check(game.players[2].state.name == "Bot 3" && game.players[2].state.character == 2 &&
              game.players[3].state.name == "Bot 4" && game.players[3].state.character == 0,
          "Runtime bots did not receive role names and recognizable characters");
    game.players[2].state.alive = false;
    game.players[2].state.health = 0;
    game.players[2].state.kills = 9;
    game.setBots(0);
    check(game.join(2, "Charlie", 1), "Removed bot slot could not be reused");
    check(game.players[2].state.alive && game.players[2].state.health == 100 &&
              game.players[2].state.kills == 0,
          "Reused bot slot retained dead/combat state");
    game.setBots(1);
    check(game.botMask() == 8 && game.players[0].human && game.players[2].human,
          "Noncontiguous bot slots corrupted humans");
    bool rejected = false;
    try {
        game.setBots(2);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected && game.settings.bots == 1, "Bot overflow was not transactional");
    game.players[3].state.vel = {50, 10};
    game.setAI(false);
    check(game.players[3].state.vel == sf::Vector2f{}, "Idle bots kept moving");
    game.players[0].state.kills = 8;
    game.players[0].state.deaths = 5;
    game.players[0].state.alive = false;
    game.restartRound();
    for (const auto& player : game.players)
        if (player.state.connected)
            check(player.state.alive && player.state.health == 100 && !player.state.kills &&
                      !player.state.deaths,
                  "Round restart did not reset scores/health");
    check(game.players[0].state.name == "Alice" && game.players[0].state.team == 1 && game.players[2].human,
          "Round restart lost identity");
    check(game.players[3].state.name == "Bot 4" && game.players[3].state.character == 0,
          "Restart lost bot role identity");
    check(game.players[0].state.character == 6, "Restart discarded selected appearance");
    {
        common::ServerSettings rosterSettings;
        rosterSettings.map = "arena";
        rosterSettings.slots = 4;
        rosterSettings.bots = 3;
        rosterSettings.botAI = false;
        GameSimulation initial(rosterSettings, logger);
        for (unsigned i = 0; i < 3; ++i)
            check(initial.players[i].state.character == i &&
                      initial.players[i].state.name == common::botName(i),
                  "Initial bots did not receive all three roles");
        GameSimulation replacement(rosterSettings, logger, nullptr, false);
        check(initial.join(3, "Appearance visitor", 0), "Appearance visitor failed to join");
        initial.players[3].state.character = 5;
        replacement.restoreRoster(initial);
        check(replacement.players[3].human && replacement.players[3].state.character == 5,
              "Map reload discarded selected human appearance");
        for (unsigned i = 0; i < 3; ++i)
            check(!replacement.players[i].human && replacement.players[i].state.character == i &&
                      replacement.players[i].state.name == initial.players[i].state.name,
                  "Map reload lost bot role identity");
    }
    game.settings.teams = 0;
    rejected = false;
    try {
        game.changeTeam(0, 1);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, "FFA allowed a team change");
}
