#pragma once
#include "roof_region.hpp"
#include <cmath>
#include <tmxlite/Map.hpp>
namespace common {
inline float sortOffset(const std::vector<tmx::Property>& properties) {
    for (const auto& property : properties) {
        if (property.getName() != "sort_offset_y")
            continue;
        float value = 0;
        if (property.getType() == tmx::Property::Type::Int)
            value = float(property.getIntValue());
        else if (property.getType() == tmx::Property::Type::Float)
            value = property.getFloatValue();
        else
            throw std::runtime_error("sort_offset_y must be a number");
        if (!std::isfinite(value))
            throw std::runtime_error("sort_offset_y must be finite");
        return value;
    }
    return 0;
}
inline float sceneryDepth(const tmx::Map& map, const tmx::Layer& layer, unsigned cell, unsigned gid) {
    const float halfHeight = map.getTileSize().y * .5f;
    if (const auto roof = roofRegion(layer))
        return (roof->position.x + roof->position.y + roof->size.x + roof->size.y - 1) * halfHeight +
               sortOffset(layer.getProperties());
    float offset = sortOffset(layer.getProperties());
    for (const auto& tileset : map.getTilesets())
        if (tileset.hasTile(gid)) {
            if (const auto* tile = tileset.getTile(gid))
                offset += sortOffset(tile->properties);
            break;
        }
    return (cell % map.getTileCount().x + cell / map.getTileCount().x + 1) * halfHeight +
           layer.getOffset().y + offset;
}
} // namespace common
