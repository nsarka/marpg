#pragma once
#include <tmxlite/Tileset.hpp>

namespace common {
// Older maps were authored against the engine's centered-image convention.
// New pivot-authored tilesets use Tiled's tile-layer bottom-left anchoring.
inline float tileAlignmentCorrection(const tmx::Tileset& set,float imageWidth,float mapWidth) {
    for(const auto& property:set.getProperties())
        if(property.getName()=="tile_layer_alignment" && property.getType()==tmx::Property::Type::String && property.getStringValue()=="bottom_left")
            return (imageWidth-mapWidth)*0.5f;
    return 0.f;
}
}
