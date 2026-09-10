#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>
#include <optional>

/*
    Camera camera({1280.f, 720.f});
    camera.setFollowSharpness(8.f);
    camera.setDeadZone({60.f, 40.f});
    camera.setWorldBounds(sf::FloatRect({-2000.f, -2000.f}, {4000.f, 4000.f}));

    while (window.isOpen()) {
        if (Player* target = chooseCameraTarget(players, localPlayerIndex)) {
            camera.follow(target->renderPosition());
        } else {
            camera.clearFollowTarget();
        }

        camera.update(renderDt);
        window.setView(camera.view());
    }
*/

class Camera {
  public:
    Camera() = default;
    explicit Camera(const sf::Vector2f& size);

    void setSize(const sf::Vector2f& size);
    void setCenter(const sf::Vector2f& center);
    void snapTo(const sf::Vector2f& center);

    void setZoom(float zoom);
    float zoom() const;

    void setFollowSharpness(float sharpness);
    void setDeadZone(const sf::Vector2f& halfExtents);
    void clearDeadZone();

    void setWorldBounds(const sf::FloatRect& bounds);
    void clearWorldBounds();

    void follow(const sf::Vector2f& target);
    void clearFollowTarget();

    void update(float dtSeconds);

    const sf::View& view() const;
    sf::View& view();

    sf::Vector2f center() const;
    sf::Vector2f size() const;

  private:
    sf::Vector2f clampCenterToBounds(const sf::Vector2f& desiredCenter) const;
    sf::Vector2f applyDeadZone(const sf::Vector2f& currentCenter, const sf::Vector2f& target) const;

  private:
    sf::View m_view;
    float m_zoom = 1.f;
    float m_followSharpness = 10.f;

    std::optional<sf::Vector2f> m_followTarget;
    std::optional<sf::Vector2f> m_deadZoneHalfExtents;
    std::optional<sf::FloatRect> m_worldBounds;
};
