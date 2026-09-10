#pragma once
#include <SFML/System/Vector2.hpp>
#include <tmxlite/Tileset.hpp>
namespace common {
inline constexpr float CharacterScale=2.f;
inline constexpr float CharacterFeetX=.5f;
inline constexpr float CharacterFeetY=.6875f;
inline bool isPlayerAnimation(const tmx::Tileset& tileset,std::uint32_t gid) {
    if(const auto* tile=tileset.getTile(gid))
        for(const auto& property:tile->properties)
            if(property.getName()=="player_animation" && property.getType()==tmx::Property::Type::Boolean)
                return property.getBoolValue();
    return false;
}
inline void applyCharacterTileLayout(sf::Vector2f frameSize,sf::Vector2f mapTileSize,
                                    sf::Vector2f& drawSize,sf::Vector2f& offset) {
    drawSize=frameSize*CharacterScale;
    // MapLayer normally bottom-centers the image; move its feet to cell center.
    offset.x+=drawSize.x*(.5f-CharacterFeetX);
    offset.y+=drawSize.y*(1.f-CharacterFeetY)-mapTileSize.y*.5f;
}
}
