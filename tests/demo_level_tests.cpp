#include "common/collision_world.hpp"
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <set>
#include <stdexcept>
#include <iostream>

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    check(argc == 2, "Map path required");
    tmx::Map map;
    check(map.load(argv[1]), "Demo map must load");
    std::set<unsigned> used;
    for (const auto& layer : map.getLayers()) {
        if (layer->getType() != tmx::Layer::Type::Tile) continue;
        for (auto tile : layer->getLayerAs<tmx::TileLayer>().getTiles())
            if (tile.ID) used.insert(tile.ID);
    }
    for (unsigned gid = 13; gid <= 23; ++gid) check(used.count(gid), "Missing tileset item");
    common::CollisionWorld world;
    world.load(argv[1]);
    check(world.size() == 13, "Expected collision footprints for every solid exhibit");
    check(!world.overlaps({-140,620}), "Spawn must be clear");
    // Image-space footprint samples transformed exactly like the level renderer.
    auto point = [](int x,int y,float px,float py) {
        return sf::Vector2f{float((x-y)*128)+px, float((x+y)*64-384)+py};
    };
    for (auto p : {point(2,2,70,420), point(8,2,180,438), point(2,5,190,476),
                   point(5,5,128,448), point(8,5,110,424), point(2,8,128,448),
                   point(5,8,128,448), point(8,8,128,448)})
        check(world.overlaps(p,0), "Solid exhibit missing footprint");
    check(world.overlaps(point(5,2,32,448),0), "Door left post must be solid");
    check(world.overlaps(point(5,2,128,400),0), "Door right post must be solid");
    auto start = point(5,2,50,364);
    auto end = world.move(start,{60,120});
    check((end-start-sf::Vector2f{60,120}).length() < 0.01f, "Door opening must be passable");
    check(!world.overlaps(point(5,10,128,448)), "Floor switch must be walkable");
    std::cout << "PASS: all 11 items, solid footprints, clear spawn, doorway passage, walkable switch\n";
}
