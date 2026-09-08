#pragma once

#include <SFML/System/Vector2.hpp>
#include <string>
#include <vector>

namespace common {
// World coordinates match MapLayer's bottom-centered isometric tile placement.
class CollisionWorld {
public:
    static constexpr float PlayerRadius = 10.f;
    void load(const std::string& mapPath, bool triggersOnly = false);
    void addPolygon(std::vector<sf::Vector2f> points);
    sf::Vector2f move(sf::Vector2f position, sf::Vector2f displacement,
                      float radius = PlayerRadius,
                      const std::vector<sf::Vector2f>& playerCenters = {}) const;
    bool overlaps(sf::Vector2f position, float radius = PlayerRadius) const;
    std::vector<std::vector<sf::Vector2f>> outlines() const;
    std::size_t size() const { return polygons_.size(); }
private:
    struct Plane { sf::Vector2f normal; float distance; };
    struct Polygon { std::vector<Plane> planes; std::vector<sf::Vector2f> points; };
    std::vector<Polygon> polygons_;
};
}
