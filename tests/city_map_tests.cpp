#include "client/world_scene.hpp"
#include "common/navigation.hpp"
#include "common/team_spawns.hpp"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
sf::Vector2f center(int x, int y) {
    return {float((x - y) * 64 + 64), float((x + y) * 32 + 32)};
}
int main(int argc, char** argv) try {
    check(argc == 2, "Repository path required");
    const auto root = std::filesystem::path(argv[1]);
    common::LoadedMap world((root / "assets/tiled/city.tmx").string());
    const auto& map = world.data();
    check(map.getTilesets().size() == 10, "City category tilesets missing");
    common::CollisionWorld walls;
    walls.load(map);
    common::TriggerSystem triggers;
    triggers.load(map);
    common::TeamSpawns spawns;
    spawns.load(map, 2, walls, triggers);
    check(spawns.groups().size() == 2 && spawns.groups()[0].size() == 16 && spawns.groups()[1].size() == 16,
          "City spawns missing");
    for (const auto& group : spawns.groups())
        for (auto point : group)
            check(triggers.hasFloor(point) && !walls.overlaps(point), "Unsafe city spawn");
    common::Navigation navigation(map, walls, triggers);
    const auto& buttons = triggers.characterSelect().buttons();
    check(buttons.size() == 11, "City must offer seven appearances and four classes");
    common::PlayerState visitor;
    visitor.connected = true;
    visitor.health = 47;
    visitor.explosionCooldown = 100;
    visitor.name = "Visitor";
    visitor.team = 1;
    for (unsigned i = 0; i < buttons.size(); ++i) {
        const auto& button = buttons[i];
        check(button.label == (button.changesClass ? common::PlayerClasses[button.value].name
                                                   : common::CharacterNames[button.value]),
              "Button label does not match its character");
        check(triggers.hasFloor(button.center) && !walls.overlaps(button.center) &&
                  !triggers.contains(button.center),
              "Appearance button is blocked or hazardous");
        check(!navigation.path(center(25, 25), button.center).empty(), "Appearance button is unreachable");
        visitor.pos = button.center;
        visitor.character = button.changesClass ? 6 : (button.value + 1) % 7;
        visitor.playerClass = button.changesClass ? static_cast<common::PlayerClass>((button.value + 1) % 4)
                                                  : common::PlayerClass::Charger;
        check(triggers.characterSelect().update(visitor) &&
                  (button.changesClass ? int(visitor.playerClass) == button.value && visitor.character == 6
                                       : visitor.character == button.value &&
                                             visitor.playerClass == common::PlayerClass::Charger),
              "Standing on a button did not select its appearance");
        check(!triggers.characterSelect().update(visitor), "Standing still repeatedly reselected character");
        check(visitor.health == 47 && visitor.explosionCooldown == 100 && visitor.team == 1 &&
                  visitor.name == "Visitor",
              "Appearance button reset combat or player identity");
        sf::Packet packet;
        common::writePlayerState(packet, visitor);
        common::PlayerState replica;
        check(common::readPlayerState(packet, replica) && replica.character == visitor.character &&
                  replica.playerClass == visitor.playerClass,
              "Selected appearance did not replicate");
    }
    visitor.character = 0;
    visitor.playerClass = common::PlayerClass::Adventurer;
    visitor.pos = buttons.back().center;
    visitor.alive = false;
    check(!triggers.characterSelect().update(visitor) && visitor.character == 0,
          "Dead players may not select an appearance");
    visitor.alive = true;
    visitor.connected = false;
    check(!triggers.characterSelect().update(visitor), "Disconnected players selected an appearance");
    visitor.connected = true;
    visitor.pos = center(25, 25);
    check(!triggers.characterSelect().update(visitor), "Appearance changed outside the button area");
    check(navigation.clear(center(19, 19), center(54, 54)), "Castle-to-farms avenue blocked");
    check(!navigation.path(center(25, 25), center(50, 50)).empty(), "Bots cannot cross city");
    check(walls.overlaps(center(14, 14)), "Castle keep collision missing");
    const std::vector<sf::Vector2i> buildings{{21, 30}, {30, 21}, {21, 39}, {39, 21},
                                              {32, 43}, {43, 32}, {38, 52}, {52, 38}};
    for (const auto& building : buildings) {
        const auto inside = center(building.x + 1, building.y + 2);
        check(triggers.hasFloor(inside) && !walls.overlaps(inside), "House interior is blocked");
        check(
            navigation.clear(center(building.x - 1, building.y + 2), center(building.x + 4, building.y + 2)),
            "Cannot walk through both house doors");
        check(!navigation.path(inside, center(34, 34)).empty(), "House is disconnected from city streets");
    }
    std::size_t roofPieces = 0;
    std::set<std::pair<float, float>> roofRegions;
    for (const auto& layer : map.getLayers()) {
        const auto region = common::roofRegion(*layer);
        if (!region)
            continue;
        roofRegions.emplace(region->position.x, region->position.y);
        check(common::roofScale(*layer) == 1.f, "City roofs must use native-scale tiles");
        check(!common::layerCollisionEnabled(*layer), "Roof objects must not block rooms");
        for (const auto& tile : layer->getLayerAs<tmx::TileLayer>().getTiles())
            roofPieces += tile.ID != 0;
    }
    check(roofRegions.size() == buildings.size(), "Each prefab needs its own translated roof region");
    check(roofPieces == buildings.size() * 28, "Incomplete prefab slopes or gables");
    check(!map.getAnimatedTiles().empty(), "Animated city objects missing");
    std::set<unsigned> distinct;
    for (const auto& layer : map.getLayers())
        if (layer->getType() == tmx::Layer::Type::Tile)
            for (const auto& tile : layer->getLayerAs<tmx::TileLayer>().getTiles())
                if (tile.ID)
                    distinct.insert(tile.ID);
    check(distinct.size() >= 100, "City lacks tile variety");
    sf::Font font;
    check(font.openFromFile(root / "assets/fonts/PixelPurl.ttf"), "Font missing");
    WorldScene scene(world, walls, triggers, font);
    scene.update(sf::milliseconds(160));
    auto draw = [&](sf::FloatRect area, const char* name, sf::Vector2u size) {
        sf::RenderTexture target(size);
        target.setView(sf::View(area));
        target.clear(sf::Color(27, 35, 26));
        scene.drawGround(target);
        scene.drawStructures(target);
        target.display();
        check(target.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path() / name),
              "City preview failed");
    };
    draw({{-3800, 100}, {7600, 4000}}, "marpg-city.png", {1900, 1000});
    draw({{-1500, 430}, {3100, 1450}}, "marpg-city-castle.png", {1550, 725});
    draw({{-1500, 1570}, {3100, 1600}}, "marpg-city-town.png", {1550, 800});
    draw({{600, 800}, {1900, 1250}}, "marpg-city-character-select.png", {1900, 1250});
    draw({{-1500, 2650}, {3100, 1300}}, "marpg-city-farms.png", {1550, 650});
    scene.setViewerPosition(center(22, 32));
    check(scene.hiddenRoofCount() == 1, "Entering a house must hide all roof slopes and gables");
    draw({{-1100, 1400}, {1300, 800}}, "marpg-city-interior.png", {1300, 800});
    scene.setViewerPosition(center(34, 34));
    check(scene.hiddenRoofCount() == 0, "Leaving a house must restore its roof");
    std::cout << "PASS: city spawns, avenue, navigation, collision, tile variety and rendering\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
