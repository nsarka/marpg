#include "input_manager.hpp"

#include <cmath>

namespace {
    void normalize2D(float& x, float& y) {
        const float lenSq = x * x + y * y;

        if (lenSq > 1.f) {
            const float invLen = 1.f / std::sqrt(lenSq);
            x *= invLen;
            y *= invLen;
        }
    }
}

InputManager::InputManager(sf::RenderWindow& window)
    : window_(window) {}

void InputManager::handleEvents() {
    edges_.jabPressed = false;
    edges_.jabReleased = false;

    edges_.hookPressed = false;
    edges_.hookReleased = false;

    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        }
        else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            setKey(key->code, true);
        }
        else if (const auto* key = event->getIf<sf::Event::KeyReleased>()) {
            setKey(key->code, false);
        }
        else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            setMouseButton(mouse->button, true);
        }
        else if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
            setMouseButton(mouse->button, false);
        }
        else if (event->is<sf::Event::FocusLost>()) {
            clearAll();
        }
    }
}

common::InputCommand InputManager::buildCommand() {
    common::InputCommand cmd;
    cmd.sequence = nextSequence_;

    if (keys_.left)   cmd.move.x -= 1.f;
    if (keys_.right)  cmd.move.x += 1.f;
    if (keys_.up)     cmd.move.y -= 1.f;
    if (keys_.down)   cmd.move.y += 1.f;
    if (keys_.sprint) cmd.sprint = true;

    normalize2D(cmd.move.x, cmd.move.y);

    cmd.jabHeld = keys_.jab;
    cmd.jabPressed = edges_.jabPressed;
    cmd.jabReleased = edges_.jabReleased;

    cmd.hookHeld = keys_.hook;
    cmd.hookPressed = edges_.hookPressed;
    cmd.hookReleased = edges_.hookReleased;

    seq_buffer.insert(nextSequence_, cmd);
    nextSequence_++;

    return cmd;
}

const InputManager::KeyState& InputManager::keys() const {
    return keys_;
}

void InputManager::clearAll() {
    keys_ = {};
    edges_ = {};
}

void InputManager::setKey(sf::Keyboard::Key key, bool pressed) {
    if (key == sf::Keyboard::Key::W) {
        keys_.up = pressed;
    }
    else if (key == sf::Keyboard::Key::S) {
        keys_.down = pressed;
    }
    else if (key == sf::Keyboard::Key::A) {
        keys_.left = pressed;
    }
    else if (key == sf::Keyboard::Key::D) {
        keys_.right = pressed;
    }
    else if (key == sf::Keyboard::Key::LShift) {
        keys_.sprint = pressed;
    }
}

void InputManager::setMouseButton(sf::Mouse::Button button, bool pressed) {
    switch (button) {
        case sf::Mouse::Button::Left:
            if (pressed) {
                edges_.jabPressed = true;
                keys_.jab = true;
            } else {
                edges_.jabReleased = true;
                keys_.jab = false;
            }
            break;

        case sf::Mouse::Button::Right:
            if (pressed) {
                edges_.hookPressed = true;
                keys_.hook = true;
            } else {
                edges_.hookReleased = true;
                keys_.hook = false;
            }
            break;

        default:
            break;
    }
}
