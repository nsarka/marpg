#pragma once
#include "load_progress.hpp"
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
    Navigation(const std::string& mapPath, const CollisionWorld& walls, const TriggerSystem& hazards,
               LoadProgress* progress = nullptr)
        : walls_(walls), hazards_(hazards) {
        LoadedMap loaded(mapPath, progress);
        const auto& map = loaded.data();
        initialize(map, progress);
    }
    Navigation(const tmx::Map& map, const CollisionWorld& walls, const TriggerSystem& hazards,
               LoadProgress* progress = nullptr)
        : walls_(walls), hazards_(hazards) {
        initialize(map, progress);
    }

  private:
    void initialize(const tmx::Map& map, LoadProgress* progress) {
        const tmx::Layer* floor = nullptr;
        for (const auto& layer : map.getLayers())
            if (layer->getType() == tmx::Layer::Type::Tile && layer->getName() == "Floor") {
                floor = layer.get();
                break;
            }
        if (!floor)
            throw std::runtime_error("Navigation requires a Floor layer");
        const auto size = floor->getSize();
        const auto tile = map.getTileSize();
        const auto offset = floor->getOffset();
        origin_ = {float(offset.x) - (size.y - 1) * tile.x * .5f, float(offset.y)};
        width_ = static_cast<int>(std::ceil((size.x + size.y) * tile.x * .5f / Spacing)) + 1;
        height_ = static_cast<int>(std::ceil((size.x + size.y) * tile.y * .5f / Spacing)) + 1;
        if (width_ <= 0 || height_ <= 0 || std::int64_t(width_) * height_ > 1000000)
            throw std::runtime_error("Invalid navigation dimensions");

        // Keep a compact node list only for occupied floor. The inexpensive grid
        // lookup preserves the fixed lattice and its eight-neighbor connectivity.
        std::vector<int> grid(width_ * height_, -1);
        std::vector<int> cells;
        const auto& floorTiles = floor->getLayerAs<tmx::TileLayer>().getTiles();
        // Visit lattice samples under each painted diamond, not the surrounding
        // empty rectangle. Adjacent diamonds share samples, so deduplicate them.
        for (std::size_t i = 0; i < floorTiles.size(); ++i) {
            if (i % 128 == 0)
                loading(progress, "Navigation: occupied floor", i, floorTiles.size());
            if (!floorTiles[i].ID)
                continue;
            const float topX = float(offset.x) + (float(i % size.x) - float(i / size.x) + 1.f) * tile.x * .5f;
            const float topY = float(offset.y) + (float(i % size.x) + float(i / size.x)) * tile.y * .5f;
            const int minX = std::max(0, int(std::ceil((topX - tile.x * .5f - origin_.x) / Spacing)));
            const int maxX =
                std::min(width_ - 1, int(std::floor((topX + tile.x * .5f - origin_.x) / Spacing)));
            const int minY = std::max(0, int(std::ceil((topY - origin_.y) / Spacing)));
            const int maxY = std::min(height_ - 1, int(std::floor((topY + tile.y - origin_.y) / Spacing)));
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x) {
                    const int cell = y * width_ + x;
                    if (grid[cell] != -1)
                        continue;
                    const auto position = origin_ + sf::Vector2f{float(x) * Spacing, float(y) * Spacing};
                    if (!hazards_.hasFloor(position))
                        continue;
                    grid[cell] = -2;
                    cells.push_back(cell);
                }
        }
        std::sort(cells.begin(), cells.end());
        nodes_.reserve(cells.size());
        for (int cell : cells) {
            grid[cell] = int(nodes_.size());
            Node node;
            node.position =
                origin_ + sf::Vector2f{float(cell % width_) * Spacing, float(cell / width_) * Spacing};
            node.neighbors.fill(-1);
            nodes_.push_back(node);
        }
        loading(progress, "Navigation: occupied floor", floorTiles.size(), floorTiles.size());
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (i % 32 == 0)
                loading(progress, "Navigation: walkable nodes", i, nodes_.size());
            nodes_[i].walkable = safePoint(nodes_[i].position);
        }
        loading(progress, "Navigation: walkable nodes", nodes_.size(), nodes_.size());
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (i % 16 == 0)
                loading(progress, "Navigation: connections", i, nodes_.size());
            auto& node = nodes_[i];
            if (!node.walkable)
                continue;
            const int x = cells[i] % width_, y = cells[i] / width_;
            int slot = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy)
                        continue;
                    const int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < width_ && ny < height_) {
                        const int next = grid[ny * width_ + nx];
                        if (next >= 0 && nodes_[next].walkable && clear(node.position, nodes_[next].position))
                            node.neighbors[slot] = next;
                    }
                    ++slot;
                }
        }
        loading(progress, "Navigation: connections", nodes_.size(), nodes_.size());
    }

  public:
    std::size_t nodeCount() const {
        return nodes_.size();
    }
    std::size_t gridCellCount() const {
        return std::size_t(width_) * height_;
    }
    bool safePoint(sf::Vector2f p) const {
        // Keep the whole footprint on floor and out of hazardous regions.
        for (auto offset : std::array<sf::Vector2f, 5>{{{0, 0}, {10, 0}, {-10, 0}, {0, 10}, {0, -10}}})
            if (hazards_.contains(p + offset))
                return false;
        return !walls_.overlaps(p);
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
