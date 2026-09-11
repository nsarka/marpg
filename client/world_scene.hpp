#pragma once
#include "client/rendering/map_layer.hpp"
#include "common/loaded_map.hpp"
#include "common/scenery_depth.hpp"
#include "common/trigger_system.hpp"
#include "ui_font.hpp"
#include "wall_occlusion.hpp"

class WorldScene {
    TileLighting lighting_;
    sf::Texture lightShadows_, stairShadows_;
    std::vector<std::unique_ptr<MapLayer>> ground_, structures_;
    WallOcclusion wallOcclusion_;
    std::map<const MapLayer*, std::pair<std::size_t, sf::FloatRect>> roofs_;
    std::set<std::size_t> hiddenRoofs_;
    sf::Vector2f tileSize_;
    struct SceneryPiece {
        MapLayer* layer;
        unsigned x, y;
    };
    struct SceneryEntry {
        float depth;
        std::size_t layerIndex;
        std::vector<SceneryPiece> pieces;
    };
    std::vector<SceneryEntry> scenery_;
    sf::FloatRect bounds_;
    std::vector<sf::ConvexShape> collisionDebugShapes;
    std::vector<sf::Text> levelLabels;

  public:
    WorldScene(const common::LoadedMap& world, const common::CollisionWorld& collision,
               const common::TriggerSystem& triggers, const sf::Font& font)
        : lighting_(world.data()), wallOcclusion_(world.data(), world.tileLayer("Walls")) {
        const auto& map = world.data();
        tileSize_ = sf::Vector2f(float(map.getTileSize().x), float(map.getTileSize().y));
        bool foreground = false;
        std::map<std::array<float, 4>, std::size_t> roofGroups;
        for (std::size_t i = 0; i < map.getLayers().size(); ++i) {
            const auto& layer = map.getLayers()[i];
            if (layer->getType() != tmx::Layer::Type::Tile)
                continue;
            if (layer->getName() == "Walls")
                foreground = true;
            if (!layer->getVisible())
                continue;
            auto rendered = std::make_unique<MapLayer>(map, i, &lighting_, foreground);
            if (layer->getName() == "Floor")
                bounds_ = rendered->getGlobalBounds();
            if (const auto region = common::roofRegion(*layer))
                roofs_.emplace(rendered.get(), std::make_pair(i, *region));
            if (foreground) {
                const auto& tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();
                const auto region = common::roofRegion(*layer);
                for (unsigned cell = 0; cell < tiles.size(); ++cell) {
                    if (!tiles[cell].ID)
                        continue;
                    const float depth = common::sceneryDepth(map, *layer, cell, tiles[cell].ID);
                    const SceneryPiece piece{rendered.get(), cell % map.getTileCount().x,
                                             cell / map.getTileCount().x};
                    if (region) {
                        const std::array<float, 4> key{region->position.x, region->position.y, region->size.x,
                                                       region->size.y};
                        auto [group, inserted] = roofGroups.emplace(key, scenery_.size());
                        if (inserted)
                            scenery_.push_back({depth, i, {}});
                        scenery_[group->second].pieces.push_back(piece);
                    } else
                        scenery_.push_back({depth, i, {piece}});
                }
            }
            if (foreground && layer->getName() != "Walls")
                wallOcclusion_.addLayer(map, i);
            (foreground ? structures_ : ground_).push_back(std::move(rendered));
        }
        std::stable_sort(scenery_.begin(), scenery_.end(),
                         [](const auto& a, const auto& b) { return a.depth < b.depth; });
        for (const auto& points : collision.outlines()) {
            sf::ConvexShape shape(points.size());
            for (std::size_t i = 0; i < points.size(); ++i)
                shape.setPoint(i, points[i]);
            shape.setFillColor(sf::Color(255, 70, 90, 55));
            shape.setOutlineColor(sf::Color(255, 80, 100));
            shape.setOutlineThickness(1.5f);
            collisionDebugShapes.push_back(std::move(shape));
        }
        for (const auto& region : triggers.regions().all()) {
            for (const auto& points : region->shape.outlines()) {
                sf::ConvexShape shape(points.size());
                for (std::size_t i = 0; i < points.size(); ++i)
                    shape.setPoint(i, points[i]);
                auto fill = region->color;
                fill.a = 40;
                shape.setFillColor(fill);
                shape.setOutlineColor(region->color);
                shape.setOutlineThickness(1.5f);
                collisionDebugShapes.push_back(std::move(shape));
            }
        }
        for (const auto& mapLayer : map.getLayers()) {
            if (mapLayer->getType() != tmx::Layer::Type::Object || mapLayer->getName() != "Labels")
                continue;
            for (const auto& object : mapLayer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
                sf::Text label(font, object.getName(), uiFontSize(18));
                const auto p = object.getPosition();
                const float scale = float(map.getTileSize().x) / (2.f * map.getTileSize().y);
                label.setPosition({(p.x - p.y) * scale, (p.x + p.y) * 0.5f});
                const auto bounds = label.getLocalBounds();
                label.setOrigin({bounds.position.x + bounds.size.x * 0.5f, 0.f});
                label.setFillColor(sf::Color(245, 235, 200));
                label.setOutlineColor(sf::Color(25, 25, 30));
                label.setOutlineThickness(2.f);
                levelLabels.push_back(std::move(label));
            }
        }
        lighting_.buildHeightField(lightShadows_, stairShadows_);
        update(sf::Time::Zero);
    }
    const sf::FloatRect& bounds() const {
        return bounds_;
    }
    TileLighting& lighting() {
        return lighting_;
    }
    void update(sf::Time dt) {
        for (auto& layer : ground_)
            layer->update(dt);
        for (auto& layer : structures_)
            layer->update(dt);
    }
    void setViewerPosition(sf::Vector2f position) {
        hiddenRoofs_.clear();
        const auto x = (position.x - tileSize_.x * .5f) / tileSize_.x;
        const auto y = position.y / tileSize_.y;
        for (const auto& [layer, roof] : roofs_)
            if (roof.second.contains({x + y, y - x}))
                hiddenRoofs_.insert(roof.first);
    }
    std::size_t hiddenRoofCount() const {
        std::set<std::pair<float, float>> regions;
        for (const auto& [layer, roof] : roofs_)
            if (hiddenRoofs_.count(roof.first))
                regions.emplace(roof.second.position.x, roof.second.position.y);
        return regions.size();
    }
    void prepareOcclusion(sf::Vector2u size, const sf::View& view, sf::Shader& shader) {
        wallOcclusion_.update(size, view, hiddenRoofs_);
        shader.setUniform("wallDepth", wallOcclusion_.texture());
        shader.setUniform("renderSize", sf::Glsl::Vec2(size));
    }
    void drawGround(sf::RenderTarget& target) const {
        for (const auto& layer : ground_)
            target.draw(*layer);
    }
    void drawStructures(sf::RenderTarget& target) const {
        for (const auto& entry : scenery_) {
            if (hiddenRoofs_.count(entry.layerIndex))
                continue;
            for (const auto& piece : entry.pieces)
                piece.layer->drawTile(target, piece.x, piece.y);
        }
        for (const auto& label : levelLabels)
            target.draw(label);
    }
    void drawDebug(sf::RenderTarget& target) const {
        for (const auto& shape : collisionDebugShapes)
            target.draw(shape);
    }
};
