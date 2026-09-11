#pragma once
#include "collision_world.hpp"
#include "common.hpp"
#include "damage.hpp"
#include "loaded_map.hpp"
#include "settings.hpp"
#include "teleport_system.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <unordered_map>

namespace common {
// Non-solid, feet-position regions. Every player/region pair has its own timer.
class TriggerSystem {
    std::shared_ptr<TriggerRegions> regions_ = std::make_shared<TriggerRegions>();
    ServerSettings settings_;
    TeleportSystem teleports_{regions_};

  public:
    const TriggerRegions& regions() const {
        return *regions_;
    }
    void load(const std::string& mapPath, const ServerSettings& settings = ServerSettings{}) {
        LoadedMap loaded(mapPath);
        const auto& map = loaded.data();
        load(map, settings);
    }
    void load(const tmx::Map& map, const ServerSettings& settings = ServerSettings{}) {
        settings_ = settings;
        teleports_.load(map);
        boundsDamage_ = settings.boundsDamage;
        boundsInterval_ = damageInterval(settings.boundsIntervalSeconds);
        CollisionWorld regions;
        regions.load(map, true);
        bool foundFloor = false;
        for (const auto& layer : map.getLayers()) {
            if (layer->getType() != tmx::Layer::Type::Tile || layer->getName() != "Floor")
                continue;
            floorSize_ = layer->getSize();
            floorOffset_ = layer->getOffset();
            tileSize_ = map.getTileSize();
            floorTiles_.clear();
            for (const auto& tile : layer->getLayerAs<tmx::TileLayer>().getTiles())
                floorTiles_.push_back(tile.ID != 0);
            foundFloor = true;
            break;
        }
        if (!foundFloor)
            throw std::runtime_error("Map requires a Floor tile layer");
        floorLoaded_ = true;
        voidElapsed_.clear();
        zones_.clear();
        for (const auto& layer : map.getLayers()) {
            if (layer->getType() != tmx::Layer::Type::Object || layer->getName() != "Triggers")
                continue;
            for (const auto& object : layer->getLayerAs<tmx::ObjectGroup>().getObjects())
                if (object.getClass() == "DamageTrigger")
                    addDamageTrigger(triggerObjectPoints(map, *layer, object), settings.triggerDamage,
                                     damageInterval(settings.triggerIntervalSeconds));
        }
        for (auto points : regions.outlines())
            addDamageTrigger(std::move(points), settings.triggerDamage,
                             damageInterval(settings.triggerIntervalSeconds));
    }
    void addDamageTrigger(std::vector<sf::Vector2f> points, int damage, Tick interval) {
        if (damage < 0 || interval == 0)
            throw std::invalid_argument("Invalid damage trigger settings");
        Zone zone;
        zone.region = regions_->add(std::move(points), sf::Color(255, 180, 40));
        zone.damage = damage;
        zone.interval = interval;
        zones_.push_back(std::move(zone));
    }
    void reset(PlayerId id) {
        teleports_.reset(id);
        resetDamage(id);
    }
    void resetDamage(PlayerId id) {
        voidElapsed_.erase(id);
        for (auto& zone : zones_)
            zone.elapsed.erase(id);
    }
    bool hasFloor(sf::Vector2f position) const {
        if (!floorLoaded_)
            return true;
        // Invert the renderer's isometric projection, including its half-tile X origin.
        const double x = (position.x - floorOffset_.x - tileSize_.x * 0.5) / tileSize_.x;
        const double y = (position.y - floorOffset_.y) / tileSize_.y;
        const double column = std::floor(y + x), row = std::floor(y - x);
        if (!std::isfinite(column) || !std::isfinite(row) || column < 0 || row < 0 ||
            column >= floorSize_.x || row >= floorSize_.y)
            return false;
        const auto index = static_cast<std::size_t>(row) * floorSize_.x + static_cast<std::size_t>(column);
        return index < floorTiles_.size() && floorTiles_[index];
    }
    bool contains(sf::Vector2f position) const {
        if (!hasFloor(position))
            return true;
        for (const auto& zone : zones_)
            if (zone.region->contains(position))
                return true;
        return false;
    }
    void update(PlayerId id, PlayerState& player, const CollisionWorld& walls) {
        if (teleports_.update(id, player, walls))
            resetDamage(id);
        update(id, player);
    }
    void update(PlayerId id, PlayerState& player) {
        if (!player.connected || !player.alive) {
            reset(id);
            return;
        }
        if (!hasFloor(player.pos)) {
            for (auto& zone : zones_)
                zone.elapsed.erase(id);
            beat(id, player, voidElapsed_, boundsDamage_, boundsInterval_);
            return;
        }
        voidElapsed_.erase(id);
        for (auto& zone : zones_) {
            if (!player.connected || !player.alive || !zone.region->contains(player.pos)) {
                zone.elapsed.erase(id);
                continue;
            }
            beat(id, player, zone.elapsed, zone.damage, zone.interval);
        }
    }
    std::vector<std::vector<sf::Vector2f>> outlines() const {
        std::vector<std::vector<sf::Vector2f>> result;
        for (const auto& zone : zones_)
            result.push_back(zone.region->shape.outlines().front());
        return result;
    }

  private:
    void beat(PlayerId id, PlayerState& player, std::unordered_map<PlayerId, Tick>& elapsed, int damage,
              Tick interval) {
        auto [timer, entered] = elapsed.try_emplace(id, 0);
        if (entered || ++timer->second >= interval) {
            timer->second = 0;
            applyDamage(player, damage, -1, player.pos, settings_);
        }
    }
    int boundsDamage_ = 2;
    Tick boundsInterval_ = TICK_RATE / 2;
    bool floorLoaded_ = false;
    tmx::Vector2u floorSize_{}, tileSize_{};
    tmx::Vector2i floorOffset_{};
    std::vector<bool> floorTiles_;
    std::unordered_map<PlayerId, Tick> voidElapsed_;
    struct Zone {
        std::shared_ptr<const TriggerRegion> region;
        int damage = 2;
        Tick interval = TICK_RATE / 2;
        std::unordered_map<PlayerId, Tick> elapsed;
    };
    std::vector<Zone> zones_;
};
} // namespace common
