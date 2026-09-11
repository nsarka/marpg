#include "common/map_composition.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pugixml.hpp>
#include <sstream>
#include <stdexcept>
namespace fs = std::filesystem;
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream(path) << text;
}
int main(int argc, char** argv) {
    check(argc == 2, "Repository path required");
    auto dir =
        fs::temp_directory_path() /
        ("marpg-prefabs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            fs::remove_all(path);
        }
    } cleanup{dir};
    auto tileset = fs::relative(fs::path(argv[1]) / "assets/tiled/fantasy_roofs.tsx", dir).generic_string();
    const std::string head =
        "<map orientation='isometric' width='4' height='4' tilewidth='128' tileheight='64'>";
    const std::string refs = "<tileset firstgid='99' source='" + tileset + "'/>";
    const std::string marker = "<object x='64' y='64'><point/><properties><property name='prefab' "
                               "type='file' value='house.tmx'/></properties></object>";
    write(dir / "house.tmx",
          head + refs +
              "<group offsetx='0' offsety='0'><layer name='Walls' width='1' height='1'><data "
              "encoding='csv'>2147483747</data></layer><objectgroup name='Roof' "
              "draworder='index'><properties><property name='collision' type='bool' value='false'/><property "
              "name='roof_region' value='0 0 1 1'/></properties><object gid='99' x='128' y='0' width='256' "
              "height='256'/></objectgroup></group></map>");
    write(dir / "map.tmx", head + refs + "<objectgroup name='Prefabs'>" + marker +
                               "<object x='128' y='128'><point/><properties><property name='prefab' "
                               "value='house.tmx'/></properties></object></"
                               "objectgroup><group><properties><property name='editor_only' type='bool' "
                               "value='true'/></properties><layer name='BadPreview'/></group></map>");
    pugi::xml_document doc;
    check(doc.load_string(common::composeMap((dir / "map.tmx").string()).c_str()),
          "Cannot parse composed map");
    auto map = doc.child("map");
    check(std::distance(map.children("tileset").begin(), map.children("tileset").end()) == 1,
          "Shared tileset duplicated");
    auto walls = map.find_child_by_attribute("layer", "name", "Walls");
    const std::string data = walls.child("data").text().get();
    check(data == "0,0,0,0,0,2147483649,0,0,0,0,2147483649,0,0,0,0,0",
          "Prefab translation or flipped GID remapping failed");
    unsigned roofs = 0;
    for (auto layer : map.children("layer")) {
        check(std::string(layer.attribute("name").value()) != "BadPreview",
              "Editor preview loaded into gameplay");
        if (std::string(layer.attribute("name").value()) != "Roof")
            continue;
        ++roofs;
        // Compare the resulting image origin with Tiled's bottom-center
        // object anchor, expressed in the game's tile-layer coordinate origin.
        std::string cells = layer.child("data").text().get();
        std::replace(cells.begin(), cells.end(), ',', ' ');
        std::istringstream input(cells);
        unsigned gid = 0, cell = 0;
        while (input >> gid && !gid)
            ++cell;
        const double imageX =
            (int(cell % 4) - int(cell / 4)) * 64 - 64 + layer.attribute("offsetx").as_double();
        const double imageY =
            (int(cell % 4) + int(cell / 4)) * 32 - 178 + layer.attribute("offsety").as_double();
        check(imageX == 0 && imageY == -114 + 64 * int(roofs - 1),
              "Tile objects must use Tiled's anchoring relative to painted tiles");
        auto region = layer.child("properties").find_child_by_attribute("property", "name", "roof_region");
        check(std::string(region.attribute("value").value()) == (roofs == 1 ? "1 1 1 1" : "2 2 1 1"),
              "Roof region not translated per instance");
    }
    check(roofs == 2, "Missing roof object groups");
    auto rejects = [&](const std::string& contents, const char* message) {
        write(dir / "house.tmx", contents);
        bool rejected = false;
        try {
            common::composeMap((dir / "map.tmx").string());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected, message);
    };
    rejects(head + refs + "<objectgroup name='Prefabs'>" + marker + "</objectgroup></map>",
            "Cyclic prefab accepted");
    rejects(head + refs + "<objectgroup><object gid='99' width='512' height='256'/></objectgroup></map>",
            "Scaled roof silently accepted");
    rejects(head + refs + "<objectgroup><object gid='99' rotation='45'/></objectgroup></map>",
            "Rotated roof silently accepted");
    rejects(head + refs +
                "<objectgroup name='Prefabs'><object x='1' y='0'><point/><properties><property name='prefab' "
                "value='missing.tmx'/></properties></object></objectgroup></map>",
            "Unsnapped prefab accepted");
    std::cout
        << "PASS: prefab reuse, GIDs, translations, roof groups, preview exclusion and invalid references\n";
}
