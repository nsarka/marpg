#include "common/team_spawns.hpp"

#include "client/world_scene.hpp"
#include "common/loaded_map.hpp"
#include "common/navigation.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
sf::Vector2f center(int x, int y) {
    return {float((x - y) * 64 + 64), float((x + y) * 32 + 32)};
}
int main(int argc, char** argv) {
    check(argc == 2, "Repository path required");
    const auto root = std::filesystem::path(argv[1]);
    common::LoadedMap world((root / "assets/tiled/farms.tmx").string());
    const auto& map = world.data();
    common::CollisionWorld walls;
    walls.load(map);
    common::TriggerSystem triggers;
    triggers.load(map);
    common::TeamSpawns spawns;
    spawns.load(map, 2, walls, triggers);
    check(spawns.groups().size() == 2 && spawns.groups()[0].size() == 16 && spawns.groups()[1].size() == 16,
          "Farm team clusters missing");
    for (auto& group : spawns.groups())
        for (auto point : group)
            check(triggers.hasFloor(point) && !walls.overlaps(point), "Unsafe farm spawn");
    common::Navigation navigation(map, walls, triggers);
    for (int y : {7, 20, 33}) {
        check(navigation.clear(center(9, y), center(52, y)), "Connecting road blocked");
        check(!navigation.path(center(9, y), center(52, y)).empty(), "Bots cannot traverse road");
    }
    for (int x : {9, 52})
        check(navigation.clear(center(x, 7), center(x, 33)), "Farm approach blocked");
    check(walls.overlaps(center(25, 13)), "Forest belt collision missing");
    check(!map.getAnimatedTiles().empty(), "Windmill animation missing");
    sf::Font font;
    check(font.openFromFile(root / "assets/fonts/PixelPurl.ttf"), "Font missing");
    WorldScene scene(world, walls, triggers, font);
    auto draw = [&](sf::FloatRect area, const char* name, sf::Vector2u size) {
        sf::RenderTexture target(size);
        target.setView(sf::View(area));
        target.clear(sf::Color(36, 43, 31));
        scene.drawGround(target);
        scene.drawStructures(target);
        target.display();
        check(target.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path() / name),
              "Cannot save preview");
    };
    draw({{-2650, -160}, {6850, 3650}}, "marpg-farms.png", {1644, 876});
    draw({{-1850, 360}, {2450, 1500}}, "marpg-farms-detail.png", {1470, 900});
    std::cout << "PASS: farm clusters, three walkable roads, bot navigation, forest collision, roofs and "
                 "scenery rendering\n";
}
