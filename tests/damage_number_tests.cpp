#include "client/damage_numbers.hpp"
#include "common/damage.hpp"
#include <iostream>
#include <stdexcept>
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main() {
    std::vector<common::PlayerState> players(3);
    for (auto& p : players)
        p.connected = true;
    DamageNumbers numbers(42);
    common::applyDamage(players[1], 10, 0, {100, 200});
    common::applyDamage(players[0], 2, -1, {20, 30});
    common::applyDamage(players[2], 15, 1, {200, 200});
    numbers.observe(players, 0, false);
    check(numbers.numbers().size() == 2, "Only incoming and local outgoing damage should display");
    const auto incoming = numbers.numbers()[0], outgoing = numbers.numbers()[1];
    check(incoming.amount == 2 && incoming.incoming && DamageNumbers::color(incoming).r > 200,
          "Incoming damage is red");
    check(outgoing.amount == 10 && !outgoing.incoming && DamageNumbers::color(outgoing) == sf::Color::White,
          "Outgoing damage is white");
    check(DamageNumbers::label(incoming) == "-2" && DamageNumbers::label(outgoing) == "-10",
          "All damage numbers show health loss with a minus sign");
    check(outgoing.origin.x >= 88 && outgoing.origin.x <= 112 && outgoing.origin.y >= 180 &&
              outgoing.origin.y <= 192,
          "Damage spawns randomly above contact within bounds");
    numbers.observe(players, 0, false);
    check(numbers.numbers().size() == 2, "Repeated snapshots must not duplicate numbers");
    common::applyDamage(players[1], 15, 0, {100, 200});
    numbers.observe(players, 0, false);
    check(numbers.numbers().size() == 3 && numbers.numbers()[2].origin != outgoing.origin,
          "Separate hits get their own random locations");
    numbers.update(0.45f);
    check(DamageNumbers::position(numbers.numbers()[1]).y < outgoing.origin.y &&
              DamageNumbers::color(numbers.numbers()[1]).a < 150,
          "Numbers rise and fade");
    numbers.update(0.46f);
    check(numbers.numbers().empty(), "Expired numbers are removed");
    numbers.observe(players, 0, false);
    check(numbers.numbers().empty(), "Old events cannot reappear after fading");
    DamageNumbers allCombat(42);
    allCombat.observe(players, 0);
    check(allCombat.numbers().size() == 4, "Enabled option must include third-party combat");
    check(allCombat.numbers().back().amount == 15 &&
              DamageNumbers::color(allCombat.numbers().back()) == sf::Color::White,
          "Third-party combat must be white");
    common::applyDamage(players[2], 2, -1, {200, 200});
    allCombat.observe(players, 0);
    check(allCombat.numbers().size() == 4, "Option must not add other players' environmental damage");
    std::cout
        << "PASS: attribution, colors, contact offset, per-hit variation, deduplication, rise and fade\n";
}
