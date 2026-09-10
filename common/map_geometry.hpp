#pragma once
#include <SFML/System/Vector2.hpp>
namespace common {
inline sf::Vector2f tileToWorld(float column, float row, sf::Vector2f tileSize) {
    return {(column - row) * tileSize.x * .5f, (column + row) * tileSize.y * .5f};
}
inline sf::Vector2f tileImagePosition(sf::Vector2f base, sf::Vector2f tileSize, sf::Vector2f imageSize,
                                      sf::Vector2f offset = {}) {
    return base + sf::Vector2f{(tileSize.x - imageSize.x) * .5f, tileSize.y - imageSize.y} + offset;
}
} // namespace common
