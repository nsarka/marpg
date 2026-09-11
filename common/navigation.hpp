#pragma once
#include "loaded_map.hpp"
#include "trigger_system.hpp"
#include <array>
#include <limits>
#include <queue>

namespace common {
// A static, feet-sized navigation grid. Edges are swept against actual wall polygons.
class Navigation {
  public:
    static constexpr float Spacing = 24.f;
    Navigation(const std::string& mapPath, const CollisionWorld& walls, const TriggerSystem& hazards)
        : walls_(walls), hazards_(hazards) {
        LoadedMap loaded(mapPath);
        const auto& map = loaded.data();
        initialize(map);
    }
    Navigation(const tmx::Map& map, const CollisionWorld& walls, const TriggerSystem& hazards)
        : walls_(walls), hazards_(hazards) {
        initialize(map);
    }

  private:
    void initialize(const tmx::Map& map) {
        for (const auto& layer : map.getLayers()) {
            if (layer->getType() != tmx::Layer::Type::Tile || layer->getName() != "Floor")
                continue;
            auto size = layer->getSize();
            auto tile = map.getTileSize();
            auto offset = layer->getOffset();
            origin_ = {float(offset.x) - (size.y - 1) * tile.x * .5f, float(offset.y)};
            width_ = static_cast<int>(std::ceil((size.x + size.y) * tile.x * .5f / Spacing)) + 1;
            height_ = static_cast<int>(std::ceil((size.x + size.y) * tile.y * .5f / Spacing)) + 1;
        }
        if (width_ <= 0 || height_ <= 0 || std::int64_t(width_) * height_ > 1000000)
            throw std::runtime_error("Invalid navigation dimensions");
        nodes_.resize(width_ * height_);
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x) {
                auto& node = nodes_[y * width_ + x];
                node.position = origin_ + sf::Vector2f{float(x) * Spacing, float(y) * Spacing};
                node.walkable = safePoint(node.position);
                node.neighbors.fill(-1);
            }
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x) {
                auto& node = nodes_[y * width_ + x];
                if (!node.walkable)
                    continue;
                int slot = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (!dx && !dy)
                            continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < width_ && ny >= 0 && ny < height_) {
                            int index = ny * width_ + nx;
                            if (nodes_[index].walkable && clear(node.position, nodes_[index].position))
                                node.neighbors[slot] = index;
                        }
                        ++slot;
                    }
            }
    }

  public:
    bool safePoint(sf::Vector2f p) const {
        if (walls_.overlaps(p))
            return false;
        // Keep the whole footprint on floor and out of hazardous regions.
        for (auto offset : std::array<sf::Vector2f, 5>{{{0, 0}, {10, 0}, {-10, 0}, {0, 10}, {0, -10}}})
            if (hazards_.contains(p + offset))
                return false;
        return true;
    }
    bool clear(sf::Vector2f from, sf::Vector2f to) const {
        if ((walls_.move(from, to - from) - to).length() > .05f)
            return false;
        int steps = std::max(1, static_cast<int>(std::ceil((to - from).length() / 8.f)));
        for (int i = 0; i <= steps; ++i) {
            auto p = from + (to - from) * (float(i) / steps);
            for (auto offset : std::array<sf::Vector2f, 5>{{{0, 0}, {10, 0}, {-10, 0}, {0, 10}, {0, -10}}})
                if (hazards_.contains(p + offset))
                    return false;
        }
        return true;
    }
    std::vector<sf::Vector2f> path(sf::Vector2f start, sf::Vector2f goal,
                                   const std::vector<sf::Vector2f>& people = {}) const {
        if (!safePoint(goal))
            return {};
        const int source = nearest(start), target = nearest(goal);
        if (source < 0 || target < 0)
            return {};
        const float inf = std::numeric_limits<float>::infinity();
        std::vector<float> costs(nodes_.size(), inf);
        std::vector<int> parent(nodes_.size(), -1);
        using Entry = std::pair<float, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
        costs[source] = 0;
        open.push({(nodes_[source].position - nodes_[target].position).length(), source});
        std::vector<bool> closed(nodes_.size());
        while (!open.empty()) {
            int current = open.top().second;
            open.pop();
            if (closed[current])
                continue;
            closed[current] = true;
            if (current == target) {
                std::vector<sf::Vector2f> result{goal};
                for (int n = target; n != -1; n = parent[n])
                    result.push_back(nodes_[n].position);
                std::reverse(result.begin(), result.end());
                return result;
            }
            for (int next : nodes_[current].neighbors) {
                if (next < 0 || closed[next])
                    continue;
                float crowd = 0;
                for (auto p : people) {
                    float d = (p - nodes_[next].position).length();
                    if (d < 48)
                        crowd += (48 - d) * 3;
                }
                float cost =
                    costs[current] + (nodes_[next].position - nodes_[current].position).length() + crowd;
                if (cost < costs[next]) {
                    costs[next] = cost;
                    parent[next] = current;
                    open.push({cost + (nodes_[next].position - nodes_[target].position).length(), next});
                }
            }
        }
        return {};
    }

  private:
    int nearest(sf::Vector2f p) const {
        int best = -1;
        float distance = Spacing * Spacing * 16;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            float d = (nodes_[i].position - p).lengthSquared();
            if (nodes_[i].walkable && d < distance && clear(p, nodes_[i].position)) {
                best = i;
                distance = d;
            }
        }
        return best;
    }
    struct Node {
        sf::Vector2f position;
        bool walkable = false;
        std::array<int, 8> neighbors;
    };
    const CollisionWorld& walls_;
    const TriggerSystem& hazards_;
    sf::Vector2f origin_{};
    int width_ = 0, height_ = 0;
    std::vector<Node> nodes_;
};
} // namespace common
