#include "input_manager.hpp"

void InputManager::handleEvents() {
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        }

        if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            setKey(key->code, true);
        } else if (const auto* key = event->getIf<sf::Event::KeyReleased>()) {
            setKey(key->code, false);
        } else if (event->is<sf::Event::FocusLost>()) {
            clear();
        }
    }
}

sf::Vector2f InputManager::movement() const {
    sf::Vector2f dir{0.f, 0.f};

    if (up_) {
        dir.y -= 1.f;
    }
    if (down_) {
        dir.y += 1.f;
    }
    if (left_) {
        dir.x -= 1.f;
    }
    if (right_) {
        dir.x += 1.f;
    }

    if (dir.x != 0.f || dir.y != 0.f) {
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        dir /= len;
    }

    return dir;
}


void InputManager::setKey(sf::Keyboard::Key key, bool pressed) {
    if (key == sf::Keyboard::Key::W) {
        up_ = pressed;
    } else if (key == sf::Keyboard::Key::S) {
        down_ = pressed;
    } else if (key == sf::Keyboard::Key::A) {
        left_ = pressed;
    } else if (key == sf::Keyboard::Key::D) {
        right_ = pressed;
    }
}

void InputManager::clear() {
    up_ = false;
    down_ = false;
    left_ = false;
    right_ = false;
}