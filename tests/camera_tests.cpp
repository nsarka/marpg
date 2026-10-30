#include "client/camera.hpp"
#include <cmath>
#include <stdexcept>
void check(bool ok) { if (!ok) throw std::runtime_error("Camera zoom regression"); }
int main() {
    Camera camera({1000, 600});
    camera.scrollZoom(1, .5f, 2.f);
    check(camera.zoom() < 1 && camera.size().x < 1000);
    camera.scrollZoom(-1, .5f, 2.f);
    check(std::abs(camera.zoom() - 1) < .00001f);
    camera.scrollZoom(.5f, .5f, 2.f);
    check(camera.zoom() < 1 && camera.zoom() > 1 / 1.1f);
    camera.scrollZoom(1000, .5f, 2.f);
    check(camera.zoom() == .5f && camera.size().x == 500);
    camera.scrollZoom(-1000, .5f, 2.f);
    check(camera.zoom() == 2 && camera.size().x == 2000);
    camera.scrollZoom(1, .75f, .75f);
    check(camera.zoom() == .75f);
    camera.setWorldBounds(sf::FloatRect({0, 0}, {3000, 3000}));
    camera.snapTo({2900, 2900});
    camera.scrollZoom(-100, .5f, 2.f);
    check(camera.center().x == 2000 && camera.center().y == 2400);
}
