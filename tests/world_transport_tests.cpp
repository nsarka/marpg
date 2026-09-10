#include "common/character_roster.hpp"
#include "common/settings.hpp"
#include "common/world_transport.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::optional<sf::Packet> deliver(common::WorldAssembler& receiver, sf::Packet packet) {
    std::string type;
    packet >> type;
    check(type == common::MSG_WORLD_PART, "Wrong envelope");
    return receiver.accept(packet);
}
void writeProtocol7Player(sf::Packet& packet, const common::PlayerState& player) {
    packet << player.team << player.connected << player.alive << player.pos.x << player.pos.y << player.vel.x
           << player.vel.y << player.name << player.health << player.score
           << static_cast<std::uint8_t>(player.lastAttack) << player.attackSequence
           << static_cast<std::uint8_t>(player.combatDebug.attack) << player.combatDebug.elapsedTicks
           << player.combatDebug.direction.x << player.combatDebug.direction.y << player.combatDebug.hit
           << player.combatDebug.target << player.damageSequence
           << static_cast<std::uint8_t>(player.damageEvents.size());
    for (const auto& event : player.damageEvents)
        packet << event.sequence << event.amount << event.source << event.contact.x << event.contact.y;
    packet << player.kills << player.deaths << player.spellSequence << player.spellPosition.x
           << player.spellPosition.y << static_cast<std::uint8_t>(player.spellEffect) << player.facing.x
           << player.facing.y << player.stunTicks << player.explosionCooldown << player.lightningCooldown
           << player.pingMs << player.teleportSequence << player.character;
}

int main() {
    std::vector<common::PlayerState> players(common::MAX_PLAYERS);
    for (auto& player : players) {
        player.connected = true;
        player.name = std::string(64, 'x');
        for (unsigned i = 1; i <= 8; ++i)
            player.damageEvents.push_back({i, 2, -1, {100, 200}});
    }
    std::mt19937 random(42);
    std::array<bool, 7> seen{};
    for (int i = 0; i < 200; ++i)
        for (int team = 0; team < 2; ++team) {
            auto character = common::chooseCharacter(team, random);
            seen[character] = true;
            check(team == 0 ? character < 3 : character >= 3 && character < 7, "Character escaped team pool");
        }
    for (bool available : seen)
        check(available, "Character variant is never selected");
    for (std::size_t i = 0; i < players.size(); ++i)
        players[i].character = i % 7;
    auto& detailed = players[0];
    detailed.kills = 7;
    detailed.deaths = 3;
    detailed.spellSequence = 41;
    detailed.spellPosition = {123.f, 456.f};
    detailed.spellEffect = common::AttackKind::Lightning;
    detailed.facing = {0.f, -1.f};
    detailed.stunTicks = 30;
    detailed.explosionCooldown = 45;
    detailed.lightningCooldown = 90;
    detailed.pingMs = 87;
    detailed.teleportSequence = 19;
    auto parts = common::worldPackets(players, 1);
    check(parts.size() > 1, "Test must exercise multiple datagrams");
    for (const auto& part : parts)
        check(part.getDataSize() <= 1200, "Datagram exceeds safe budget");
    common::WorldAssembler receiver;
    check(!deliver(receiver, parts.back()), "Partial snapshot published");
    check(!deliver(receiver, parts.back()), "Duplicate completed snapshot");
    std::optional<sf::Packet> result;
    for (std::size_t i = parts.size() - 1; i-- > 0;)
        result = deliver(receiver, parts[i]);
    check(result.has_value(), "Reordered snapshot not assembled");
    std::string type;
    *result >> type;
    std::vector<common::PlayerState> decoded;
    check(type == common::MSG_WORLD && common::readWorldPacket(*result, decoded), "Snapshot decode failed");
    for (std::size_t i = 0; i < players.size(); ++i)
        check(decoded[i].character == players[i].character, "Character selection lost in snapshot");
    check(decoded.size() == 32 && decoded.back().damageEvents.size() == 8 &&
              decoded.back().name == players.back().name,
          "Roundtrip changed state");
    const auto& restored = decoded[0];
    check(restored.kills == 7 && restored.deaths == 3 && restored.spellSequence == 41 &&
              restored.spellPosition == sf::Vector2f{123.f, 456.f} &&
              restored.spellEffect == common::AttackKind::Lightning &&
              restored.facing == sf::Vector2f{0.f, -1.f} && restored.stunTicks == 30 &&
              restored.explosionCooldown == 45 && restored.lightningCooldown == 90 && restored.pingMs == 87 &&
              restored.teleportSequence == 19,
          "Complete player state lost fields");
    sf::Packet playerWire;
    common::writePlayerState(playerWire, detailed);
    sf::Packet protocol7Wire;
    writeProtocol7Player(protocol7Wire, detailed);
    check(playerWire.getDataSize() == protocol7Wire.getDataSize() &&
              std::memcmp(playerWire.getData(), protocol7Wire.getData(), playerWire.getDataSize()) == 0,
          "Player serialization changed the protocol 7 wire layout");
    sf::Packet shortPlayer;
    shortPlayer.append(playerWire.getData(), playerWire.getDataSize() - 1);
    common::PlayerState unchanged;
    unchanged.name = "Preserve me";
    check(!common::readPlayerState(shortPlayer, unchanged) && unchanged.name == "Preserve me",
          "Truncated player footer mutated state");
    auto invalidCharacter = detailed;
    invalidCharacter.character = 255;
    sf::Packet invalidPlayer;
    common::writePlayerState(invalidPlayer, invalidCharacter);
    check(!common::readPlayerState(invalidPlayer, unchanged) && unchanged.name == "Preserve me",
          "Invalid player footer mutated state");
    sf::Packet wrongVersion;
    wrongVersion << std::uint32_t(common::ProtocolVersion - 1);
    check(!common::readWorldPacket(wrongVersion, decoded) && decoded[0].kills == 7,
          "Wrong protocol accepted");
    sf::Packet extra;
    common::writeWorldPacket(extra, players);
    extra << std::uint8_t(0);
    extra >> type;
    check(!common::readWorldPacket(extra, decoded) && decoded[0].kills == 7, "Trailing world bytes accepted");
    for (auto part : parts)
        check(!deliver(receiver, part), "Stale snapshot replayed");
    auto lost = common::worldPackets(players, 2);
    for (std::size_t i = 1; i < lost.size(); ++i)
        check(!deliver(receiver, lost[i]), "Missing part accepted");
    auto fresh = common::worldPackets(players, 3);
    for (auto part : fresh)
        result = deliver(receiver, part);
    check(result.has_value(), "Dropped old snapshot blocked fresh one");
    check(!deliver(receiver, lost[0]), "Late old snapshot rolled state back");
    sf::Packet malformed = parts[0];
    malformed.append("x", 1);
    common::WorldAssembler invalid;
    check(!deliver(invalid, malformed), "Malformed payload accepted");
    sf::Packet truncated;
    truncated << true;
    auto previous = decoded;
    check(!common::readWorldPacket(truncated, decoded) && decoded[0].name == previous[0].name,
          "Partial decode mutated world");
    std::cout << "PASS: bounded datagrams, 32-player roundtrip, reordering, duplicates, loss recovery, stale "
                 "and malformed packets\n";
}
