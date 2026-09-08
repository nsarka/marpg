#include "common/common.hpp"

#include <algorithm>

namespace common {

static std::unordered_map<AttackKind, AttackDesc> kAttackTable{
    {AttackKind::Jab, {
        .startupTicks = 16,
        .activeTicks = 8,
        .recoveryTicks = 40,
        .range = 70.f,
        .damage = 20
    }},
    {AttackKind::Hook, {
        .startupTicks = 24,
        .activeTicks = 8,
        .recoveryTicks = 32,
        .range = 80.f,
        .damage = 35
    }},
    {AttackKind::Uppercut, {
        .startupTicks = 6,
        .activeTicks = 3,
        .recoveryTicks = 14,
        .range = 75.f,
        .damage = 20
    }}
};

const AttackDesc& attackDescription(AttackKind kind) { return kAttackTable.at(kind); }

void setAttackDamage(int jab,int hook) { kAttackTable.at(AttackKind::Jab).damage=jab;kAttackTable.at(AttackKind::Hook).damage=hook; }

void writeInputCmd(sf::Packet& packet, const common::PlayerId& id, const common::InputCommand& cmd) {
    packet << std::string(MSG_STATE);
    packet << id
           << cmd.sequence
           << cmd.move.x
           << cmd.move.y
           << cmd.sprint
           << cmd.jabHeld
           << cmd.jabPressed
           << cmd.jabReleased
           << cmd.hookHeld
           << cmd.hookPressed
           << cmd.hookReleased
           << cmd.aim.x << cmd.aim.y;
}

bool readInputCmd(sf::Packet& packet, common::PlayerId& id, common::InputCommand& cmd) {
    float moveX = 0.f;
    float moveY = 0.f;

    if (!(packet >> id >> cmd.sequence >> moveX >> moveY >> cmd.sprint >> cmd.jabHeld >> cmd.jabPressed >> cmd.jabReleased >> cmd.hookHeld >> cmd.hookPressed >> cmd.hookReleased >> cmd.aim.x >> cmd.aim.y)) {
        return false;
    }

    cmd.move.x = moveX;
    cmd.move.y = moveY;

    return true;
}

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
}

bool readPlayerState(sf::Packet& packet, PlayerState& player) {
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

    if (attack > static_cast<std::uint8_t>(AttackKind::Uppercut) ||
        debugAttack > static_cast<std::uint8_t>(AttackKind::Uppercut)) {
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

    return true;
}

void writeWorldPacket(sf::Packet& packet,
                      const std::vector<PlayerState>& players, const std::vector<KillEvent>& kills) {
    packet << std::string(MSG_WORLD);

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
    std::vector<PlayerState> snapshot(MAX_PLAYERS);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!readPlayerState(packet, snapshot[i])) {
            return false;
        }
    }

    std::vector<KillEvent> history;
    // Older snapshots have no trailing kill history; player state remains compatible.
    if(!packet.endOfPacket()) {
        std::uint8_t count;
        if(!(packet >> count) || count>KillHistorySize)return false;
        for(unsigned i=0;i<count;++i) {
            KillEvent event;std::uint8_t cause;
            if(!(packet >> event.sequence >> event.killer >> event.victim >> event.killerTeam >> event.victimTeam
                 >> event.killerName >> event.victimName >> cause) || cause>static_cast<std::uint8_t>(KillCause::Bounds) ||
                 event.victim>=MAX_PLAYERS || event.killer<-1 || event.killer>=MAX_PLAYERS ||
                 event.killerName.size()>64 || event.victimName.size()>64)return false;
            event.cause=static_cast<KillCause>(cause);history.push_back(std::move(event));
        }
    }
    if(kills)*kills=std::move(history);
    players=std::move(snapshot);
    return true;
}

//common::parseTest("../assets/tiled/Sample.tmx");
void parseTest(const char *map_path)
{
    const std::array<std::string, 4u> LayerStrings =
    {
        std::string("Tile"),
        std::string("Object"),
        std::string("Image"),
        std::string("Group"),
    };

    tmx::Map map;

    if (map.load(map_path))
    {
        std::cout << "Loaded Map version: " << map.getVersion().upper << ", " << map.getVersion().lower << std::endl;
        if (map.isInfinite())
        {
            std::cout << "Map is infinite.\n";
        }
        else
        {
            std::cout << "Map Dimensions: " << map.getBounds() << std::endl;
        }

        const auto& mapProperties = map.getProperties();
        std::cout << "Map class: " << map.getClass() << std::endl;

        std::cout << "Map tileset has " << map.getTilesets().size() << " tilesets" << std::endl;
        for (const auto& tileset : map.getTilesets()) 
        {
            std::cout << "Tileset: " << tileset.getName() << std::endl;
            std::cout << "Tileset class: " << tileset.getClass() << std::endl;
        }

        std::cout << "Map has " << mapProperties.size() << " properties" << std::endl;
        for (const auto& prop : mapProperties)
        {
            std::cout << "Found property: " << prop.getName() << std::endl;
            std::cout << "Type: " << int(prop.getType()) << std::endl;
        }

        std::cout << std::endl;

        const auto& layers = map.getLayers();
        std::cout << "Map has " << layers.size() << " layers" <<  std::endl;
        for (const auto& layer : layers)
        {
            std::cout << "Found Layer: " << layer->getName() << std::endl;
            std::cout << "Layer Type: " << LayerStrings[static_cast<std::int32_t>(layer->getType())] << std::endl;
            std::cout << "Layer Dimensions: " << layer->getSize() << std::endl;
            std::cout << "Layer Tint: " << layer->getTintColour() << std::endl;

            if (layer->getType() == tmx::Layer::Type::Group)
            {
                std::cout << "Checking sublayers" << std::endl;
                const auto& sublayers = layer->getLayerAs<tmx::LayerGroup>().getLayers();
                std::cout << "LayerGroup has " << sublayers.size() << " layers" << std::endl;
                for (const auto& sublayer : sublayers)
                {
                    std::cout << "Found Layer: " << sublayer->getName() << std::endl;
                    std::cout << "Sub-layer Type: " << LayerStrings[static_cast<std::int32_t>(sublayer->getType())] << std::endl;
                    std::cout << "Sub-layer Class: " << sublayer->getClass() << std::endl;
                    std::cout << "Sub-layer Dimensions: " << sublayer->getSize() << std::endl;
                    std::cout << "Sub-layer Tint: " << sublayer->getTintColour() << std::endl;

                    if (sublayer->getType() == tmx::Layer::Type::Object)
                    {
                        std::cout << sublayer->getName() << " has " << sublayer->getLayerAs<tmx::ObjectGroup>().getObjects().size() << " objects" << std::endl;
                    }
                    else if (sublayer->getType() == tmx::Layer::Type::Tile)
                    {
                        std::cout << sublayer->getName() << " has " << sublayer->getLayerAs<tmx::TileLayer>().getTiles().size() << " tiles" << std::endl;
                    }
                }
            }

            if(layer->getType() == tmx::Layer::Type::Object)
            {
                const auto& objects = layer->getLayerAs<tmx::ObjectGroup>().getObjects();
                std::cout << "Found " << objects.size() << " objects in layer" << std::endl;
                for(const auto& object : objects)
                {
                    std::cout << "Object " << object.getUID() << ", " << object.getName() <<  std::endl;
                    const auto& properties = object.getProperties();
                    std::cout << "Object has " << properties.size() << " properties" << std::endl;
                    for(const auto& prop : properties)
                    {
                        std::cout << "Found property: " << prop.getName() << std::endl;
                        std::cout << "Type: " << int(prop.getType()) << std::endl;
                    }

                    if (!object.getTilesetName().empty())
                    {
                        std::cout << "Object uses template tile set " << object.getTilesetName() << "\n";
                    }
                }
            }

            if (layer->getType() == tmx::Layer::Type::Tile)
            {
                const auto& tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();
                if (tiles.empty())
                {
                    const auto& chunks = layer->getLayerAs<tmx::TileLayer>().getChunks();
                    if (chunks.empty())
                    {
                        std::cout << "Layer has missing tile data\n";
                    }
                    else
                    {
                        std::cout << "Layer has " << chunks.size() << " tile chunks.\n";
                    }
                }
                else
                {
                    std::cout << "Layer has " << tiles.size() << " tiles.\n";
                }
            }

            const auto& properties = layer->getProperties();
            std::cout << properties.size() << " Layer Properties:" << std::endl;
            for (const auto& prop : properties)
            {
                std::cout << "Found property: " << prop.getName() << std::endl;
                std::cout << "Type: " << int(prop.getType()) << std::endl;
            }
        }
    }
    else
    {
        std::cout << "Failed loading map" << std::endl;
    }
}

} // namespace common