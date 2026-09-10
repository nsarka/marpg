#pragma once
#include "collision_world.hpp"
#include <SFML/Graphics/Color.hpp>
#include <memory>
#include <tmxlite/Map.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace common {
inline std::vector<sf::Vector2f> triggerObjectPoints(const tmx::Map& map,const tmx::Layer& layer,const tmx::Object& object) {
    std::vector<sf::Vector2f> points;
    if(object.getShape()==tmx::Object::Shape::Rectangle) {
        const auto b=object.getAABB();points={{0,0},{b.width,0},{b.width,b.height},{0,b.height}};
    } else if(object.getShape()==tmx::Object::Shape::Polygon) {
        for(auto p:object.getPoints())points.push_back({p.x,p.y});
    } else throw std::runtime_error("Trigger must be a rectangle or convex polygon: "+object.getName());
    const auto origin=object.getPosition();const auto offset=layer.getOffset();
    const float angle=object.getRotation()*3.14159265359f/180.f;
    for(auto& point:points) {
        const float x=origin.x+point.x*std::cos(angle)-point.y*std::sin(angle);
        const float y=origin.y+point.x*std::sin(angle)+point.y*std::cos(angle);
        // Engine screen origin is half a cell right of Tiled's top vertex.
        point={(x-y)*map.getTileSize().x/(2.f*map.getTileSize().y)+map.getTileSize().x*.5f+offset.x,(x+y)*.5f+offset.y};
    }
    return points;
}

// Creating a gameplay trigger region registers the same geometry for debug drawing.
// Behaviors own regions; the registry holds weak references so reloads cannot leave
// stale debug polygons behind.
struct TriggerRegion {
    CollisionWorld shape;
    sf::Color color;
    bool contains(sf::Vector2f point) const {return shape.overlaps(point,0.f);}
};
class TriggerRegions {
    std::vector<std::weak_ptr<const TriggerRegion>> regions_;
public:
    std::shared_ptr<const TriggerRegion> add(std::vector<sf::Vector2f> points,sf::Color color) {
        auto region=std::make_shared<TriggerRegion>();
        region->shape.addPolygon(std::move(points));region->color=color;
        regions_.erase(std::remove_if(regions_.begin(),regions_.end(),[](const auto& r){return r.expired();}),regions_.end());
        regions_.push_back(region);return region;
    }
    std::vector<std::shared_ptr<const TriggerRegion>> all() const {
        std::vector<std::shared_ptr<const TriggerRegion>> result;
        for(const auto& entry:regions_)if(auto region=entry.lock())result.push_back(std::move(region));
        return result;
    }
};
}
