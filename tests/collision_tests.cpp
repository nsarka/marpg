#include "common/collision_world.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    common::CollisionWorld world;
    world.addPolygon({{0,-100}, {10,-100}, {10,100}, {0,100}});
    auto free = world.move({-50,0}, {10,20});
    check(std::abs(free.x+40)<0.01f && std::abs(free.y-20)<0.01f, "Free movement changed");
    auto hit = world.move({-50,0}, {1000,0});
    check(hit.x <= -10 && hit.x > -10.01f, "Long step tunneled through wall");
    auto reverse = world.move({50,0}, {-1000,0});
    check(reverse.x >= 20 && reverse.x < 20.01f, "Reverse collision failed");
    auto slide = world.move({-50,0}, {100,50});
    check(slide.x <= -10 && std::abs(slide.y-50)<0.01f, "Wall sliding failed");
    auto escape = world.move(slide, {-20,0});
    check(escape.x < -29, "Cannot move away from wall");
    auto spawn = world.move({5,0}, {});
    check(!world.overlaps(spawn), "Spawn overlap unresolved");
    world.addPolygon({{-100,100}, {10,100}, {10,110}, {-100,110}});
    auto corner = world.move({-50,50}, {300,300});
    check(!world.overlaps(corner) && corner.x < -9.99f && corner.y < 90.01f, "Corner collision failed");
    common::CollisionWorld diagonal;
    diagonal.addPolygon({{0,0},{100,50},{110,30},{10,-20}});
    auto diagonalHit = diagonal.move({50,100},{0,-200});
    check(!diagonal.overlaps(diagonalHit) && diagonalHit.y > -50, "Isometric sliding failed");
    common::CollisionWorld arena;
    std::vector<sf::Vector2f> players{{0,0}};
    auto playerHit = arena.move({-50,0}, {1000,0}, 10, players);
    check(playerHit.x <= -20 && playerHit.x > -20.01f, "Tunneled through player");
    auto playerAway = arena.move(playerHit, {-10,0}, 10, players);
    check(playerAway.x < -29.9f, "Cannot move away from player");
    auto playerSlide = arena.move({-50,10}, {80,0}, 10, players);
    check(playerSlide.y > 10 && playerSlide.length() >= 20, "Cannot slide around player");
    auto overlap = arena.move({0,0}, {}, 10, players);
    check(overlap.length() >= 20, "Overlapping players remain stuck");
    // Two approaching players resolve sequentially without swapping sides.
    auto left = arena.move({-25,0}, {100,0}, 10, {{25,0}});
    auto right = arena.move({25,0}, {-100,0}, 10, {left});
    check(right.x-left.x >= 20 && right.x > left.x, "Head-on players crossed");
    arena.addPolygon({{30,-100},{40,-100},{40,100},{30,100}});
    auto blocked = arena.move({-40,0}, {100,0}, 10, {{10,0}});
    check(blocked.x <= -10 && !arena.overlaps(blocked), "Player collision pushed through wall");
    check(argc == 2, "Map path required");
    common::CollisionWorld map;
    map.load(argv[1]);
    check(map.size() == 44, "Expected 24 wall and 20 fence polygons from map");
    // First wall tile: local polygon (-0.682,447.531) plus image origin (0,-384).
    check(map.overlaps({70,35},0), "Map polygon does not match rendered wall");
    auto actualWall = map.move({70,150}, {0,-300});
    check(!map.overlaps(actualWall) && actualWall.y > -100, "Map wall collision failed");
    check(!map.overlaps({-140,620}), "Player spawn is blocked");
    std::cout << "PASS: free movement, sweeps, sliding, corners, overlap recovery, map placement\n";
}
