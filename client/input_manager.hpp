#pragma once

#include "common/common.hpp"
#include "common/keybindings.hpp"
#include <set>
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

        bool scoreboard = false; // Tab, held while the scoreboard is visible

        bool walk = false; // left shift
        bool rightWalk = false; // right shift

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
    explicit InputManager(sf::RenderWindow& window, const common::KeyBindings& bindings = {});
    ~InputManager() = default;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Poll events
    void handleEvents();

    // Produce one command for this tick
    common::InputCommand buildCommand(sf::Vector2f playerPosition, const sf::View& worldView);

    const KeyState& keys() const;

    void clearAll();
    bool collisionDebugEnabled() const { return collisionDebugEnabled_; }

private:
    void setBinding(int code, bool pressed);
    common::KeyBindings bindings_;
    std::set<int> held_;
    void setKey(sf::Keyboard::Key key, bool pressed);
    void setMouseButton(sf::Mouse::Button button, bool pressed);

private:
    sf::RenderWindow& window_;

    bool collisionDebugEnabled_ = false;
    bool debugKeyHeld_ = false;
    std::optional<sf::Vector2i> attackMousePosition_;
    KeyState keys_;
    EdgeState edges_;

    common::SequenceBuffer<common::InputCommand, 128> seq_buffer;
    std::uint32_t nextSequence_ = 0;
};
