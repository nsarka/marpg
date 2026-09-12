#include "client/loading_screen.hpp"
#include <filesystem>
#include <sstream>
int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    const std::filesystem::path root = argv[1];
    sf::Font font(root / "assets/fonts/PixelPurl.ttf");
    sf::RenderWindow window(sf::VideoMode({900, 500}), "MARPG loading test");
    std::ostringstream output;
    common::Logger logger(output);
    bool stop = false;
    LoadingScreen loading(window, font, logger, [&] { return stop; });
    auto map = loading.loadMap((root / "tests/fixtures/world.tmx").string());
    if (map->data().getTileCount().x != 12)
        return 2;
    loading.draw({"Building occlusion masks", 150, 300, 1.5, 4.2, false});
    loading.draw({"Building occlusion masks", 150, 300, 1.5, 4.2, false});
    sf::Texture capture(window.getSize());
    capture.update(window);
    if (!capture.copyToImage().saveToFile(std::filesystem::temp_directory_path() /
                                          "marpg-loading-screen.png"))
        return 3;
    bool failed = false;
    try {
        loading.loadMap((root / "tests/fixtures/missing-map.tmx").string());
    } catch (const std::runtime_error&) {
        failed = true;
    }
    if (!failed)
        return 4;
    stop = true;
    try {
        loading.loadMap((root / "tests/fixtures/world.tmx").string());
    } catch (const common::LoadingCancelled&) {
        return 0;
    }
    return 5;
}
