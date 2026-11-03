#pragma once
#include "collision_world.hpp"
#include <algorithm>

namespace common {
inline sf::Vector2f beamEndpoint(sf::Vector2f from, sf::Vector2f aim, const CollisionWorld& walls,
                                 float range) {
    const auto delta = aim - from;
    if (delta.length() < .001f)
        return from;
    const auto wanted = from + delta.normalized() * range;
    // A zero-radius ray clips at the first wall. Do not use the movement solver's
    // sliding endpoint: beams must remain straight after striking an angled wall.
    float low = 0, high = 1;
    if ((walls.move(from, wanted - from, 0.f) - wanted).length() <= .01f)
        return wanted;
    for (int i = 0; i < 20; ++i) {
        const float middle = (low + high) * .5f;
        const auto point = from + (wanted - from) * middle;
        if ((walls.move(from, point - from, 0.f) - point).length() <= .01f)
            low = middle;
        else
            high = middle;
    }
    return from + (wanted - from) * low;
}
inline bool beamContains(sf::Vector2f origin, sf::Vector2f end, sf::Vector2f target, float radius) {
    const auto ray = end - origin;
    const float length = ray.length();
    if (length < .001f)
        return false;
    const float along = (target - origin).dot(ray / length);
    return along >= 0 && along <= length &&
           (target - (origin + ray / length * along)).length() <= radius + CollisionWorld::PlayerRadius;
}
} // namespace common
