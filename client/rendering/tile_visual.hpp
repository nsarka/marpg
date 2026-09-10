#pragma once
#include "common/tile_alignment.hpp"
#include "common/character_visual.hpp"
#include "client/rendering/tile_lighting.hpp"
#include <filesystem>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>

#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace map_detail {
    struct AnimationState
    {
        sf::Vector2u tileCoords;
        sf::Time currentTime;
        tmx::Tileset::Tile animTile;
        std::uint8_t flipFlags = 0;
    };

    struct TileVisual
    {
        std::string textureKey;
        const sf::Texture* texture = nullptr;
        const sf::Texture* normal = nullptr;
        const sf::Texture* height = nullptr;
        unsigned stair=0;
        sf::Vector2f offset{};
        std::shared_ptr<sf::Shader> lightingShader;
        sf::Vector2f texTopLeft{0.f, 0.f};
        sf::Vector2f texSize{0.f, 0.f};
        sf::Vector2f drawSize{0.f, 0.f};
    };

}
