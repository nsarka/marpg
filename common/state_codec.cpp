#include "common.hpp"
#include "settings.hpp"
#include "character_roster.hpp"
#include <cmath>
namespace common {
void writePlayerState(sf::Packet& packet, const PlayerState& player) {
    packet << player.team << player.connected
           << player.alive
           << player.pos.x
           << player.pos.y
           << player.vel.x
           << player.vel.y
           << player.name
           << player.health
           << player.score
           << static_cast<std::uint8_t>(player.lastAttack)
           << player.attackSequence
           << static_cast<std::uint8_t>(player.combatDebug.attack)
           << player.combatDebug.age
           << player.combatDebug.direction.x << player.combatDebug.direction.y
           << player.combatDebug.hit << player.combatDebug.target
           << player.damageSequence << static_cast<std::uint8_t>(player.damageEvents.size());
    for (const auto& event : player.damageEvents)
        packet << event.sequence << event.amount << event.source << event.contact.x << event.contact.y;
    packet << player.kills << player.deaths << player.spellSequence << player.spellPosition.x << player.spellPosition.y
           << static_cast<std::uint8_t>(player.spellEffect) << player.facing.x << player.facing.y << player.stunTicks
           << player.explosionCooldown << player.lightningCooldown << player.pingMs << player.teleportSequence << player.character;
}

bool readPlayerState(sf::Packet& packet, PlayerState& destination) {
    PlayerState player;
    float x = 0.f;
    float y = 0.f;
    float dx = 0.f;
    float dy = 0.f;
    std::uint8_t attack = 0, debugAttack = 0;

    if (!(packet >> player.team >> player.connected >> player.alive >> x >> y >> dx >> dy >> player.name >> player.health >> player.score >> attack >> player.attackSequence
          >> debugAttack >> player.combatDebug.age
          >> player.combatDebug.direction.x >> player.combatDebug.direction.y
          >> player.combatDebug.hit >> player.combatDebug.target)) {
        return false;
    }

    if (attack > static_cast<std::uint8_t>(AttackKind::Lightning) ||
        debugAttack > static_cast<std::uint8_t>(AttackKind::Lightning)) {
        return false;
    }
    std::uint8_t damageCount=0;
    if (!(packet >> player.damageSequence >> damageCount) || damageCount>DamageHistorySize) return false;
    player.damageEvents.clear();
    for (unsigned i=0;i<damageCount;++i) {
        DamageEvent event;
        if (!(packet >> event.sequence >> event.amount >> event.source >> event.contact.x >> event.contact.y)) return false;
        player.damageEvents.push_back(event);
    }
    player.lastAttack = static_cast<AttackKind>(attack);
    player.combatDebug.attack = static_cast<AttackKind>(debugAttack);
    player.pos = {x, y};
    player.vel = {dx, dy};
    std::uint8_t spell=0;
    if(!(packet >> player.kills >> player.deaths >> player.spellSequence >> player.spellPosition.x >> player.spellPosition.y
         >> spell >> player.facing.x >> player.facing.y >> player.stunTicks >> player.explosionCooldown
         >> player.lightningCooldown >> player.pingMs >> player.teleportSequence >> player.character))return false;
    if((spell!=3 && spell!=4) || player.pingMs<-1 || player.character>=CharacterNames.size() ||
       !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx) || !std::isfinite(dy) ||
       !std::isfinite(player.facing.x) || !std::isfinite(player.facing.y) ||
       !std::isfinite(player.spellPosition.x) || !std::isfinite(player.spellPosition.y))return false;
    player.spellEffect=static_cast<AttackKind>(spell);
    destination=std::move(player);

    return true;
}

void writeWorldPacket(sf::Packet& packet,
                      const std::vector<PlayerState>& players, const std::vector<KillEvent>& kills) {
    packet << std::string(MSG_WORLD) << ProtocolVersion;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        writePlayerState(packet, players[i]);
    }
    const auto count=std::min(kills.size(),KillHistorySize);
    packet << static_cast<std::uint8_t>(count);
    for(std::size_t i=kills.size()-count;i<kills.size();++i) {
        const auto& event=kills[i];
        packet << event.sequence << event.killer << event.victim << event.killerTeam << event.victimTeam
               << event.killerName << event.victimName << static_cast<std::uint8_t>(event.cause);
    }

}

bool readWorldPacket(sf::Packet& packet,
                     std::vector<PlayerState>& players, std::vector<KillEvent>* kills) {
    std::uint32_t version=0;if(!(packet>>version) || version!=ProtocolVersion)return false;
    std::vector<PlayerState> snapshot(MAX_PLAYERS);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!readPlayerState(packet, snapshot[i])) {
            return false;
        }
    }

    std::vector<KillEvent> history;
    {
        std::uint8_t count;
        if(!(packet >> count) || count>KillHistorySize)return false;
        for(unsigned i=0;i<count;++i) {
            KillEvent event;std::uint8_t cause;
            if(!(packet >> event.sequence >> event.killer >> event.victim >> event.killerTeam >> event.victimTeam
                 >> event.killerName >> event.victimName >> cause) || cause>static_cast<std::uint8_t>(KillCause::Lightning) ||
                 event.victim>=MAX_PLAYERS || event.killer<-1 || event.killer>=MAX_PLAYERS ||
                 event.killerName.size()>64 || event.victimName.size()>64)return false;
            event.cause=static_cast<KillCause>(cause);history.push_back(std::move(event));
        }
    }
    if(!packet.endOfPacket())return false;
    if(kills)*kills=std::move(history);
    players=std::move(snapshot);
    return true;
}

} // namespace common
