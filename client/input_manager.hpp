#pragma once

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>

#include <cmath>

class InputManager {
public:
    explicit InputManager(sf::RenderWindow& window) : window_(window) {}
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void handleEvents();
    sf::Vector2f movement() const;

private:
    void setKey(sf::Keyboard::Key key, bool pressed);
    void clear();

    bool up_ = false;
    bool down_ = false;
    bool left_ = false;
    bool right_ = false;

    sf::RenderWindow& window_;
};
