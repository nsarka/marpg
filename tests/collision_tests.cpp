#include <filesystem>
#include <fstream>
#include <chrono>
#include "common/collision_world.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    {
        const auto file=std::filesystem::temp_directory_path()/("marpg-flips-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmx");
        for(unsigned flags=0;flags<8;++flags) {
            const std::uint32_t gid=1u | ((flags&1)?0x80000000u:0) | ((flags&2)?0x40000000u:0) | ((flags&4)?0x20000000u:0);
            std::ofstream out(file);
            out<<"<map version='1.10' orientation='isometric' width='1' height='1' tilewidth='256' tileheight='128' infinite='0'>"
                  "<tileset firstgid='1' name='Flip test' tilewidth='100' tileheight='200' tilecount='1' columns='0'>"
                  "<tile id='0'><image source='unused.png' width='100' height='200'/><objectgroup>"
                  "<object id='1' x='20' y='60' width='20' height='40'/>"
                  "<object id='2' class='DamageTrigger' x='20' y='60'><polygon points='0,0 20,0 20,40 0,40'/></object>"
                  "</objectgroup></tile></tileset><layer name='Walls' width='1' height='1'><data encoding='csv'>"
               <<gid<<"</data></layer></map>";
            out.close();
            // Center (30,80) becomes (40,60) under diagonal transposition.
            float x=(flags&4)?40.f:30.f,y=(flags&4)?60.f:80.f;
            if(flags&1)x=100-x;if(flags&2)y=200-y;
            const sf::Vector2f center{x+78,y-72};
            for(bool triggers:{false,true}) {
                common::CollisionWorld flipped;flipped.load(file.string(),triggers);
                check(flipped.size()==1 && flipped.overlaps(center,0),"Flipped polygon not at rendered tile position");
                check(!flipped.overlaps(center+sf::Vector2f{90,0},0),"Flipped polygon bounds incorrect");
                auto stopped=flipped.move(center+sf::Vector2f{-100,0},{200,0},0);
                check(stopped.x<center.x,"Flipped polygon did not block swept movement");
            }
        }
        std::filesystem::remove(file);
    }
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
    check(map.size()>20,"Fantasy arena collision missing");
    for(const auto& polygon:map.outlines()) {
        sf::Vector2f center{};for(auto p:polygon)center+=p;center/=float(polygon.size());
        check(map.overlaps(center,0),"Arena polygon does not contain its center");
    }
    std::cout << "PASS: free movement, sweeps, sliding, corners, overlap recovery, map placement\n";
}
