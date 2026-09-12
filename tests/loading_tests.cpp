#include "common/navigation.hpp"
#include <iostream>
#include <sstream>

void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    std::vector<common::LoadStatus> events;
    bool cancel = false;
    common::LoadProgress progress([&](const auto& s) { events.push_back(s); }, [&] { return cancel; });
    progress.report("First", 0, 10);
    progress.report("First", 10, 10);
    progress.report("Second");
    progress.finish();
    check(events.size() >= 4, "Missing stage boundaries");
    check(events[1].finished && events[1].completed == 10, "Stage completion lost final count");
    for (const auto& event : events)
        check(event.totalSeconds >= event.stageSeconds && event.stageSeconds >= 0, "Invalid timing");
    cancel = true;
    try {
        progress.report("Cancelled");
        throw std::runtime_error("Cancellation ignored");
    } catch (const common::LoadingCancelled&) {
    }

    // Two disconnected islands in a mostly empty map, with an off-center floor
    // offset. Compare compact nodes against every occupied point of the lattice.
    std::ostringstream xml;
    xml << "<map version='1.10' orientation='isometric' width='40' height='40' tilewidth='128' "
           "tileheight='64'>"
           "<layer name='Floor' width='40' height='40' offsetx='13' offsety='7'><data encoding='csv'>";
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 40; ++x) {
            if (x || y)
                xml << ',';
            xml << ((x >= 2 && x <= 6 && y >= 2 && y <= 6) || (x >= 30 && x <= 34 && y >= 30 && y <= 34) ? 1
                                                                                                         : 0);
        }
    xml << "</data></layer></map>";
    tmx::Map map;
    check(map.loadFromString(xml.str(), ""), "Fixture parse failed");
    common::CollisionWorld walls;
    common::TriggerSystem hazards;
    hazards.load(map);
    common::Navigation navigation(map, walls, hazards);
    std::size_t expected = 0;
    const int width = int(std::ceil(80 * 64.f / common::Navigation::Spacing)) + 1;
    const int height = int(std::ceil(80 * 32.f / common::Navigation::Spacing)) + 1;
    const sf::Vector2f origin{13 - 39 * 64.f, 7};
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            expected += hazards.hasFloor(origin + sf::Vector2f{float(x) * 24, float(y) * 24});
    check(navigation.nodeCount() == expected, "Compact graph missed occupied lattice samples");
    check(navigation.nodeCount() < navigation.gridCellCount() / 20, "Empty map area allocated nodes");
    check(!navigation.path({77, 199}, {77, 359}).empty(), "Connected island lost path");
    check(navigation.path({77, 199}, {77, 2055}).empty(), "Path crossed gap between islands");
    for (const auto* phase :
         {"Navigation: occupied floor", "Navigation: walkable nodes", "Navigation: connections"}) {
        bool reached = false;
        common::LoadProgress stop([&](const auto& s) { reached |= s.stage == phase; },
                                  [&] { return reached; });
        try {
            common::Navigation cancelled(map, walls, hazards, &stop);
            throw std::runtime_error("Navigation cancellation ignored");
        } catch (const common::LoadingCancelled&) {
        }
        check(reached, "Navigation phase missing");
    }
    std::cout << "Sparse navigation: " << navigation.nodeCount() << " / " << navigation.gridCellCount()
              << " nodes; loading progress and cancellation passed\n";
}
