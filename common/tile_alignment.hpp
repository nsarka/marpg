#pragma once
#include <tmxlite/Tileset.hpp>

namespace common {
// Convert bottom-left tile-layer anchoring to the renderer's centered-image coordinates.
// Tilesets without this property use centered-image anchoring.
inline float tileAlignmentCorrection(const tmx::Tileset& set, float imageWidth, float mapWidth) {
    for (const auto& property : set.getProperties())
        if (property.getName() == "tile_layer_alignment" &&
            property.getType() == tmx::Property::Type::String && property.getStringValue() == "bottom_left")
            return (imageWidth - mapWidth) * 0.5f;
    return 0.f;
}
} // namespace common
