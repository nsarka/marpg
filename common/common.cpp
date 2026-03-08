#include "common/common.hpp"

#include <algorithm>

namespace common {

float distanceSq(const sf::Vector2f& a, const sf::Vector2f& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

sf::Vector2f lerp(const sf::Vector2f& a, const sf::Vector2f& b, float t) {
    return a + (b - a) * t;
}

void clampToPlayfield(sf::Vector2f& pos, float radius) {
    pos.x = std::clamp(pos.x, radius, WINDOW_WIDTH - radius);
    pos.y = std::clamp(pos.y, radius, WINDOW_HEIGHT - radius);
}

void writePlayerState(sf::Packet& packet, const PlayerState& player) {
    packet << player.connected
           << player.pos.x
           << player.pos.y
           << player.name
           << player.score;
}

bool readPlayerState(sf::Packet& packet, PlayerState& player) {
    float x = 0.f;
    float y = 0.f;
    if (!(packet >> player.connected >> x >> y >> player.name >> player.score)) {
        return false;
    }

    player.pos = {x, y};
    return true;
}

void writeCollectibleState(sf::Packet& packet, const CollectibleState& collectible) {
    packet << collectible.active
           << collectible.pos.x
           << collectible.pos.y;
}

bool readCollectibleState(sf::Packet& packet, CollectibleState& collectible) {
    float x = 0.f;
    float y = 0.f;
    if (!(packet >> collectible.active >> x >> y)) {
        return false;
    }

    collectible.pos = {x, y};
    return true;
}

void writeWorldPacket(sf::Packet& packet,
                      int connectedCount,
                      const std::vector<PlayerState>& players,
                      const std::vector<CollectibleState>& collectibles) {
    packet << std::string(MSG_WORLD);
    packet << connectedCount;

    packet << static_cast<int>(players.size());
    for (const auto& player : players) {
        writePlayerState(packet, player);
    }

    packet << static_cast<int>(collectibles.size());
    for (const auto& collectible : collectibles) {
        writeCollectibleState(packet, collectible);
    }
}

bool readWorldPacket(sf::Packet& packet,
                     int& connectedCount,
                     std::vector<PlayerState>& players,
                     std::vector<CollectibleState>& collectibles) {
    int playerCount = 0;
    int collectibleCount = 0;

    if (!(packet >> connectedCount >> playerCount)) {
        return false;
    }
    if (playerCount < 0) {
        return false;
    }

    players.resize(static_cast<std::size_t>(playerCount));
    for (int i = 0; i < playerCount; ++i) {
        if (!readPlayerState(packet, players[static_cast<std::size_t>(i)])) {
            return false;
        }
    }

    if (!(packet >> collectibleCount)) {
        return false;
    }
    if (collectibleCount < 0) {
        return false;
    }

    collectibles.resize(static_cast<std::size_t>(collectibleCount));
    for (int i = 0; i < collectibleCount; ++i) {
        if (!readCollectibleState(packet, collectibles[static_cast<std::size_t>(i)])) {
            return false;
        }
    }

    return true;
}

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