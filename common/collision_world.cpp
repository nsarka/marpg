#include "common/tile_alignment.hpp"
#include "collision_world.hpp"
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace common {
namespace {
float dot(sf::Vector2f a, sf::Vector2f b) { return a.x*b.x + a.y*b.y; }
constexpr float Skin = 0.001f;
}

void CollisionWorld::addPolygon(std::vector<sf::Vector2f> points) {
    if (points.size() < 3) throw std::runtime_error("Collision polygon needs at least three vertices");
    float area = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        auto a = points[i], b = points[(i+1)%points.size()];
        area += a.x*b.y - a.y*b.x;
    }
    if (std::abs(area) < Skin) throw std::runtime_error("Degenerate collision polygon");
    if (area < 0) std::reverse(points.begin(), points.end());
    Polygon polygon;
    for (std::size_t i = 0; i < points.size(); ++i) {
        auto a = points[i], edge = points[(i+1)%points.size()] - a;
        float length = std::sqrt(dot(edge, edge));
        if (length < Skin) continue;
        sf::Vector2f normal{edge.y/length, -edge.x/length};
        float distance = dot(normal, a);
        for (auto p : points) {
            if (dot(normal, p) > distance + Skin)
                throw std::runtime_error("Collision polygons must be convex; split concave shapes in Tiled");
        }
        polygon.planes.push_back({normal, distance});
    }
    polygon.points = std::move(points);
    polygons_.push_back(std::move(polygon));
}

void CollisionWorld::load(const std::string& mapPath, bool triggersOnly) {
    tmx::Map map;
    if (!map.load(mapPath)) throw std::runtime_error("Cannot load collision map: " + mapPath);
    if (map.getOrientation() != tmx::Orientation::Isometric || map.isInfinite())
        throw std::runtime_error("Collision requires a finite isometric map");
    CollisionWorld loaded;
    auto tileSize = map.getTileSize();
    for (const auto& layer : map.getLayers()) {
        if (layer->getType() != tmx::Layer::Type::Tile) continue;
        const auto& tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();
        const auto width = layer->getSize().x;
        const auto offset = layer->getOffset();
        for (std::size_t i = 0; i < tiles.size(); ++i) {
            if (!tiles[i].ID) continue;
            const tmx::Tileset::Tile* tile = nullptr;
            sf::Vector2f tileOffset{};
            for (const auto& set : map.getTilesets()) {
                if (set.hasTile(tiles[i].ID)) { tile = set.getTile(tiles[i].ID); tileOffset={float(static_cast<std::int32_t>(set.getTileOffset().x))+common::tileAlignmentCorrection(set,float(tile->imageSize.x),float(map.getTileSize().x)),float(static_cast<std::int32_t>(set.getTileOffset().y))}; break; }
            }
            if (!tile || tile->objectGroup.getObjects().empty()) continue;

            const float x = static_cast<float>(i % width), y = static_cast<float>(i / width);
            sf::Vector2f origin{
                (x-y)*tileSize.x*0.5f + (float(tileSize.x)-tile->imageSize.x)*0.5f + offset.x + tileOffset.x,
                (x+y)*tileSize.y*0.5f + float(tileSize.y)-tile->imageSize.y + offset.y + tileOffset.y};
            for (const auto& object : tile->objectGroup.getObjects()) {
                const bool isTrigger = object.getClass() == "DamageTrigger";
                if (isTrigger != triggersOnly) continue;
                std::vector<sf::Vector2f> points;
                if (object.getShape() == tmx::Object::Shape::Polygon) {
                    for (auto point : object.getPoints()) points.push_back({point.x, point.y});
                } else if (object.getShape() == tmx::Object::Shape::Rectangle) {
                    auto bounds = object.getAABB();
                    points = {{0,0}, {bounds.width,0}, {bounds.width,bounds.height}, {0,bounds.height}};
                } else {
                    throw std::runtime_error("Tile collision objects must be polygons or rectangles");
                }
                float angle = object.getRotation()*3.14159265359f/180.f;
                auto position = object.getPosition();
                for (auto& p : points) {
                    p = sf::Vector2f{position.x + p.x*std::cos(angle)-p.y*std::sin(angle),
                                     position.y + p.x*std::sin(angle)+p.y*std::cos(angle)};
                    // Match MapLayer's UV transforms, including rectangular images:
                    // transpose normalized coordinates first, then reflect X/Y.
                    const float width=tile->imageSize.x,height=tile->imageSize.y;
                    if(tiles[i].flipFlags & tmx::TileLayer::Diagonal)
                        p={p.y/height*width,p.x/width*height};
                    if(tiles[i].flipFlags & tmx::TileLayer::Horizontal)p.x=width-p.x;
                    if(tiles[i].flipFlags & tmx::TileLayer::Vertical)p.y=height-p.y;
                    p+=origin;
                }
                loaded.addPolygon(std::move(points));
            }
        }
    }
    *this = std::move(loaded);
}

std::vector<std::vector<sf::Vector2f>> CollisionWorld::outlines() const {
    std::vector<std::vector<sf::Vector2f>> result;
    for (const auto& polygon : polygons_) result.push_back(polygon.points);
    return result;
}

bool CollisionWorld::overlaps(sf::Vector2f position, float radius) const {
    for (const auto& polygon : polygons_) {
        bool inside = true;
        for (auto plane : polygon.planes)
            if (dot(plane.normal, position) >= plane.distance + radius - Skin) { inside = false; break; }
        if (inside) return true;
    }
    return false;
}

sf::Vector2f CollisionWorld::move(sf::Vector2f position, sf::Vector2f displacement, float radius,
                                  const std::vector<sf::Vector2f>& playerCenters) const {
    if (!std::isfinite(displacement.x) || !std::isfinite(displacement.y)) return position;
    // Resolve a spawn or map edit that initially puts the feet inside a wall.
    for (int pass = 0; pass < 16; ++pass) {
        bool resolved = false;
        for (const auto& polygon : polygons_) {
            float depth = std::numeric_limits<float>::max();
            sf::Vector2f normal;
            for (auto plane : polygon.planes) {
                float d = plane.distance + radius - dot(plane.normal, position);
                if (d < depth) { depth = d; normal = plane.normal; }
            }
            if (depth > 0) { position += normal*(depth+Skin); resolved = true; }
        }
        for (auto center : playerCenters) {
            auto offset = position - center;
            const float distance = std::sqrt(dot(offset, offset));
            if (distance < radius * 2.f) {
                const auto normal = distance > Skin ? offset / distance : sf::Vector2f{1,0};
                position += normal * (radius * 2.f - distance + Skin);
                resolved = true;
            }
        }
        if (!resolved) break;
    }
    // Sweep against expanded convex polygons, then project remaining motion along
    // the hit surface. Continuous sweeps prevent tunneling even on long steps.
    for (int iteration = 0; iteration < 8 && dot(displacement, displacement) > 1e-10f; ++iteration) {
        float first = 1.f;
        sf::Vector2f hitNormal;
        bool hit = false;
        for (const auto& polygon : polygons_) {
            float enter = 0.f, leave = 1.f;
            sf::Vector2f normal;
            bool possible = true;
            for (auto plane : polygon.planes) {
                float distance = dot(plane.normal, position) - plane.distance - radius;
                float speed = dot(plane.normal, displacement);
                if (std::abs(speed) < 1e-8f) {
                    if (distance > 0) { possible = false; break; }
                    continue;
                }
                float t = -distance/speed;
                if (speed < 0) {
                    if (t >= enter) { enter = t; normal = plane.normal; }
                } else leave = std::min(leave, t);
                if (enter > leave) { possible = false; break; }
            }
            if (possible && enter <= first && dot(normal, normal) > 0) {
                first = enter; hitNormal = normal; hit = true;
            }
        }
        // Other players have the same foot radius. Sweep the relative point
        // against their expanded circles, choosing the earliest wall or player hit.
        for (auto center : playerCenters) {
            const auto offset = position - center;
            const float a = dot(displacement, displacement);
            const float b = dot(offset, displacement);
            const float c = dot(offset, offset) - 4.f * radius * radius;
            if (b >= 0) continue; // Movement away from a touching player is free.
            const float discriminant = b*b - a*c;
            if (discriminant < 0) continue;
            const float t = (-b - std::sqrt(discriminant)) / a;
            if (t >= 0 && t <= first) {
                const auto offsetAtHit = offset + displacement*t;
                const float length = std::sqrt(dot(offsetAtHit, offsetAtHit));
                if (length > Skin) {
                    first = t; hitNormal = offsetAtHit / length; hit = true;
                }
            }
        }
        const float travel = hit
            ? std::max(0.f, first - Skin / std::sqrt(dot(displacement, displacement))) : first;
        position += displacement*travel;
        if (!hit) break;
        position += hitNormal*Skin;
        displacement *= 1.f-first;
        float intoWall = dot(displacement, hitNormal);
        if (intoWall < 0) displacement -= hitNormal*intoWall;
    }
    return position;
}
}
