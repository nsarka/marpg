#pragma once
#include "collision_world.hpp"
#include "common.hpp"
#include "trigger_regions.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tmxlite/Map.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <unordered_set>

namespace common {
class TeleportSystem {
    struct Portal {
        std::string name, destination;
        std::shared_ptr<const TriggerRegion> region;
        sf::Vector2f center{};
    };
    std::shared_ptr<TriggerRegions> regions_;
    std::vector<Portal> portals_;
    std::unordered_set<PlayerId> blocked_;

  public:
    explicit TeleportSystem(std::shared_ptr<TriggerRegions> regions = std::make_shared<TriggerRegions>())
        : regions_(std::move(regions)) {}
    void load(const std::string& path) {
        tmx::Map map;
        if (!map.load(path))
            throw std::runtime_error("Cannot load teleport map: " + path);
        load(map);
    }
    void load(const tmx::Map& map) {
        portals_.clear();
        blocked_.clear();
        for (const auto& layer : map.getLayers()) {
            if (layer->getType() != tmx::Layer::Type::Object || layer->getName() != "Triggers")
                continue;
            for (const auto& object : layer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
                if (object.getClass() != "Teleport")
                    continue;
                Portal portal;
                portal.name = object.getName();
                for (const auto& prop : object.getProperties())
                    if (prop.getName() == "destination" && prop.getType() == tmx::Property::Type::String)
                        portal.destination = prop.getStringValue();
                if (portal.name.empty() || portal.destination.empty())
                    throw std::runtime_error("Teleport requires a name and destination");
                for (const auto& existing : portals_)
                    if (existing.name == portal.name)
                        throw std::runtime_error("Duplicate teleport name: " + portal.name);
                auto points = triggerObjectPoints(map, *layer, object);
                for (auto point : points)
                    portal.center += point;
                portal.region = regions_->add(points, sf::Color(190, 110, 255));
                portal.center /= float(points.size());
                portals_.push_back(std::move(portal));
            }
        }
        for (const auto& p : portals_)
            if (std::none_of(portals_.begin(), portals_.end(),
                             [&](const auto& q) { return q.name == p.destination; }))
                throw std::runtime_error("Unknown teleport destination: " + p.destination);
    }
    std::vector<std::vector<sf::Vector2f>> outlines() const {
        std::vector<std::vector<sf::Vector2f>> result;
        for (const auto& portal : portals_)
            for (auto points : portal.region->shape.outlines())
                result.push_back(std::move(points));
        return result;
    }
    void reset(PlayerId id) {
        blocked_.erase(id);
    }
    bool update(PlayerId id, PlayerState& state, const CollisionWorld& walls) {
        if (!state.connected || !state.alive) {
            reset(id);
            return false;
        }
        auto source = std::find_if(portals_.begin(), portals_.end(),
                                   [&](const auto& p) { return p.region->contains(state.pos); });
        if (source == portals_.end()) {
            reset(id);
            return false;
        }
        if (blocked_.count(id))
            return false;
        const auto target = std::find_if(portals_.begin(), portals_.end(),
                                         [&](const auto& p) { return p.name == source->destination; });
        if (walls.overlaps(target->center))
            return false;
        state.pos = target->center;
        state.vel = {};
        ++state.teleportSequence;
        blocked_.insert(id);
        return true;
    }
};
} // namespace common
