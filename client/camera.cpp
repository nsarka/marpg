#include "camera.hpp"

#include <algorithm>
#include <cmath>

Camera::Camera(const sf::Vector2f& size) : m_view({0.f, 0.f}, size) {}

void Camera::setSize(const sf::Vector2f& size) {
    m_view.setSize(size);
}

void Camera::setCenter(const sf::Vector2f& center) {
    m_view.setCenter(clampCenterToBounds(center));
}

void Camera::snapTo(const sf::Vector2f& center) {
    m_view.setCenter(clampCenterToBounds(center));
}

void Camera::setZoom(float zoom) {
    if (zoom <= 0.f) {
        return;
    }

    const sf::Vector2f oldBaseSize = m_view.getSize() / m_zoom;
    m_zoom = zoom;
    m_view.setSize(oldBaseSize * m_zoom);
    m_view.setCenter(clampCenterToBounds(m_view.getCenter()));
}

float Camera::zoom() const {
    return m_zoom;
}

void Camera::setFollowSharpness(float sharpness) {
    m_followSharpness = std::max(0.f, sharpness);
}

void Camera::setDeadZone(const sf::Vector2f& halfExtents) {
    m_deadZoneHalfExtents = halfExtents;
}

void Camera::clearDeadZone() {
    m_deadZoneHalfExtents.reset();
}

void Camera::setWorldBounds(const sf::FloatRect& bounds) {
    m_worldBounds = bounds;
    m_view.setCenter(clampCenterToBounds(m_view.getCenter()));
}

void Camera::clearWorldBounds() {
    m_worldBounds.reset();
}

void Camera::follow(const sf::Vector2f& target) {
    m_followTarget = target;
}

void Camera::clearFollowTarget() {
    m_followTarget.reset();
}

void Camera::update(float dtSeconds) {
    if (!m_followTarget.has_value()) {
        return;
    }

    sf::Vector2f current = m_view.getCenter();
    sf::Vector2f desired = *m_followTarget;

    if (m_deadZoneHalfExtents.has_value()) {
        desired = applyDeadZone(current, desired);
    }

    const float alpha = 1.f - std::exp(-m_followSharpness * dtSeconds);
    sf::Vector2f next = current + (desired - current) * alpha;
    m_view.setCenter(clampCenterToBounds(next));
}

const sf::View& Camera::view() const {
    return m_view;
}

sf::View& Camera::view() {
    return m_view;
}

sf::Vector2f Camera::center() const {
    return m_view.getCenter();
}

sf::Vector2f Camera::size() const {
    return m_view.getSize();
}

sf::Vector2f Camera::applyDeadZone(const sf::Vector2f& currentCenter, const sf::Vector2f& target) const {
    const sf::Vector2f hz = *m_deadZoneHalfExtents;
    sf::Vector2f desired = currentCenter;

    const float left = currentCenter.x - hz.x;
    const float right = currentCenter.x + hz.x;
    const float top = currentCenter.y - hz.y;
    const float bottom = currentCenter.y + hz.y;

    if (target.x < left) {
        desired.x = target.x + hz.x;
    } else if (target.x > right) {
        desired.x = target.x - hz.x;
    }

    if (target.y < top) {
        desired.y = target.y + hz.y;
    } else if (target.y > bottom) {
        desired.y = target.y - hz.y;
    }

    return desired;
}

sf::Vector2f Camera::clampCenterToBounds(const sf::Vector2f& desiredCenter) const {
    if (!m_worldBounds.has_value()) {
        return desiredCenter;
    }

    const sf::FloatRect bounds = *m_worldBounds;
    const sf::Vector2f halfView = m_view.getSize() * 0.5f;

    // If the world is smaller than the view on an axis, keep centered on the world.
    sf::Vector2f result = desiredCenter;

    if (bounds.size.x <= m_view.getSize().x) {
        result.x = bounds.position.x + bounds.size.x * 0.5f;
    } else {
        result.x = std::clamp(desiredCenter.x, bounds.position.x + halfView.x,
                              bounds.position.x + bounds.size.x - halfView.x);
    }

    if (bounds.size.y <= m_view.getSize().y) {
        result.y = bounds.position.y + bounds.size.y * 0.5f;
    } else {
        result.y = std::clamp(desiredCenter.y, bounds.position.y + halfView.y,
                              bounds.position.y + bounds.size.y - halfView.y);
    }

    return result;
}
