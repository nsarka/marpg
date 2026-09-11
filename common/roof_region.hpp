#pragma once
#include <SFML/Graphics/Rect.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tmxlite/Layer.hpp>
namespace common {
inline bool layerCollisionEnabled(const tmx::Layer& layer) {
    for (const auto& p : layer.getProperties())
        if (p.getName() == "collision" && p.getType() == tmx::Property::Type::Boolean)
            return p.getBoolValue();
    return true;
}
inline std::optional<sf::FloatRect> roofRegion(const tmx::Layer& layer) {
    for (const auto& p : layer.getProperties()) {
        if (p.getName() != "roof_region" || p.getType() != tmx::Property::Type::String)
            continue;
        std::istringstream input(p.getStringValue());
        float x, y, w, h;
        if (!(input >> x >> y >> w >> h) || w <= 0 || h <= 0)
            throw std::runtime_error("Invalid roof_region: expected x y width height in map cells");
        return sf::FloatRect({x, y}, {w, h});
    }
    return std::nullopt;
}
inline float roofScale(const tmx::Layer& layer) {
    for (const auto& p : layer.getProperties()) {
        if (p.getName() != "roof_scale")
            continue;
        float scale = p.getType() == tmx::Property::Type::Int ? float(p.getIntValue()) : p.getFloatValue();
        if (!(scale >= 1 && scale <= 8) || layerCollisionEnabled(layer))
            throw std::runtime_error("roof_scale requires collision=false and a scale between 1 and 8");
        return scale;
    }
    return 1;
}
} // namespace common
