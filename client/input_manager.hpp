#pragma once

#include "common/common.hpp"
#include "common/sequence_buffer.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <cstdint>
#include <optional>

class InputManager {
public:
    struct KeyState {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;

        bool sprint = false; // shift

        bool jab = false;   // left mouse
        bool hook = false;  // right mouse
    };

    struct EdgeState {
        bool jabPressed = false;
        bool jabReleased = false;

        bool hookPressed = false;
        bool hookReleased = false;
    };

public:
    explicit InputManager(sf::RenderWindow& window);
    ~InputManager() = default;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Poll events
    void handleEvents();

    // Produce one command for this tick
    common::InputCommand buildCommand();

    const KeyState& keys() const;

    void clearAll();

private:
    void setKey(sf::Keyboard::Key key, bool pressed);
    void setMouseButton(sf::Mouse::Button button, bool pressed);

private:
    sf::RenderWindow& window_;

    KeyState keys_;
    EdgeState edges_;

    common::SequenceBuffer<common::InputCommand, 128> seq_buffer;
    std::uint32_t nextSequence_;
};
