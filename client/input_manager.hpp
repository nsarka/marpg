#pragma once
#include "common/settings.hpp"

#include "common/combat_system.hpp"
#include "common/common.hpp"
#include "common/keybindings.hpp"
#include "common/sequence_buffer.hpp"
#include <set>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <cstdint>
#include <optional>

class InputManager {
  public:
    void setSettings(const common::ServerSettings& settings) {
        settings_ = settings;
    }
    struct KeyState {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;

        bool scoreboard = false; // Tab, held while the scoreboard is visible

        bool walk = false;      // left shift
        bool rightWalk = false; // right shift

        bool light = false; // left mouse
        bool heavy = false; // right mouse
    };

    struct EdgeState {
        bool lightPressed = false;
        bool lightReleased = false;

        bool heavyPressed = false;
        bool heavyReleased = false;
    };

  public:
    explicit InputManager(sf::RenderWindow& window, const common::KeyBindings& bindings = {});
    ~InputManager() = default;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void updateSpellAvailability(const common::PlayerState& player) {
        const bool free = player.alive && player.health > 0 && player.stunTicks == 0 &&
                          player.combatDebug.attack == common::AttackKind::None;
        explosionAvailable_ = free && player.explosionCooldown == 0;
        lightningAvailable_ = free && player.lightningCooldown == 0;
        if (spellReady_ &&
            !(selectedSpell_ == common::AttackKind::Lightning ? lightningAvailable_ : explosionAvailable_))
            spellReady_ = false;
    }

    // Poll events
    void handleEvents();

    // Produce one command for this tick
    common::InputCommand buildCommand(sf::Vector2f playerPosition, const sf::View& worldView,
                                      const common::CollisionWorld& walls);

    const KeyState& keys() const;

    void clearAll();
    void cancelCombatInput() {
        keys_.light = keys_.heavy = false;
        edges_ = {};
        spellReady_ = spellClicked_ = false;
        spellClick_.reset();
        attackMousePosition_.reset();
    }
    common::AttackKind selectedSpell() const {
        return selectedSpell_;
    }
    bool spellReady() const {
        return spellReady_;
    }
    bool collisionDebugEnabled() const {
        return collisionDebugEnabled_;
    }

  private:
    common::ServerSettings settings_;
    void setBinding(int code, bool pressed);
    bool active(common::Action action) const;
    void updateMovementBindings();
    void updateAttackEdges();
    void updateDebugToggle();
    void updateSpellSelection();
    common::KeyBindings bindings_;
    std::set<int> held_;
    void setKey(sf::Keyboard::Key key, bool pressed);
    void setMouseButton(sf::Mouse::Button button, bool pressed);

  private:
    sf::RenderWindow& window_;

    bool collisionDebugEnabled_ = false;
    bool debugKeyHeld_ = false;
    bool spellKeyHeld_ = false, spellReady_ = false, spellClicked_ = false;
    common::AttackKind selectedSpell_ = common::AttackKind::Explosion,
                       clickedSpell_ = common::AttackKind::Explosion;
    bool lightningKeyHeld_ = false;
    bool explosionAvailable_ = false, lightningAvailable_ = false;
    std::optional<sf::Vector2i> spellClick_;
    std::optional<sf::Vector2i> attackMousePosition_;
    KeyState keys_;
    EdgeState edges_;

    common::SequenceBuffer<common::InputCommand, 128> seq_buffer;
    std::uint32_t nextSequence_ = 0;
};
