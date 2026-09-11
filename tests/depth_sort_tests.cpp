#include "client/world_scene.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    check(argc == 2, "Repository path required");
    const auto dir =
        fs::temp_directory_path() /
        ("marpg-depth-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            fs::remove_all(path);
        }
    } cleanup{dir};
    check(sf::Image({128, 256}, sf::Color::Green).saveToFile(dir / "green.png"), "Fixture image failed");
    check(sf::Image({128, 256}, sf::Color::Red).saveToFile(dir / "red.png"), "Fixture image failed");
    check(sf::Image({128, 256}, sf::Color::Blue).saveToFile(dir / "blue.png"), "Fixture image failed");
    std::ofstream(dir / "tiles.tsx")
        << "<tileset name='Depth' tilewidth='128' tileheight='256' tilecount='3' columns='0'>"
           "<tile id='0'><image source='green.png' width='128' height='256'/><animation><frame tileid='0' "
           "duration='50'/><frame tileid='2' duration='50'/></animation></tile>"
           "<tile id='1'><image source='red.png' width='128' height='256'/></tile>"
           "<tile id='2'><image source='blue.png' width='128' height='256'/></tile></tileset>";
    sf::Font font;
    check(font.openFromFile(fs::path(argv[1]) / "assets/fonts/PixelPurl.ttf"), "Font missing");
    auto render = [&](bool tie, bool overrideDepth, bool animated) {
        std::ofstream map(dir / "map.tmx");
        map << "<map version='1.10' orientation='isometric' width='2' height='2' tilewidth='128' tileheight='64'><tileset "
               "firstgid='1' source='tiles.tsx'/>"
               "<layer name='Floor' width='2' height='2'><data encoding='csv'>0,0,0,0</data></layer>"
               "<layer name='Walls' width='2' height='2'><data encoding='csv'>0,0,0,1</data></layer>"
               "<layer name='Back wall' width='2' height='2'>";
        if (overrideDepth)
            map << "<properties><property name='sort_offset_y' type='float' value='100'/></properties>";
        map << "<data encoding='csv'>" << (tie ? "0,0,0,2" : "2,0,0,0") << "</data></layer></map>";
        map.close();
        common::LoadedMap world((dir / "map.tmx").string());
        common::CollisionWorld walls;
        walls.load(world.data());
        common::TriggerSystem triggers;
        triggers.load(world.data());
        WorldScene scene(world, walls, triggers, font);
        if (animated)
            scene.update(sf::milliseconds(60));
        sf::RenderTexture target({512, 512});
        target.setView(sf::View(sf::FloatRect({-64, -256}, {512, 512})));
        target.clear(sf::Color::Black);
        scene.drawGround(target);
        scene.drawStructures(target);
        target.display();
        return target.getTexture().copyToImage().getPixel({128, 256});
    };
    const auto front = render(false, false, false);
    check(front.g > 150 && front.r == 0, "Later background layer covered foreground tile");
    const auto animated = render(false, false, true);
    check(animated.b > 150 && animated.r == 0, "Animation lost the sorted foreground placement");
    const auto tied = render(true, false, false);
    check(tied.r > 150 && tied.g == 0, "Equal-depth tiles must preserve layer order");
    const auto overridden = render(false, true, false);
    check(overridden.r > 150 && overridden.g == 0, "sort_offset_y did not adjust scenery ordering");
    std::cout << "PASS: cross-layer depth, stable ties, explicit offsets and animated frames\n";
}
