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
} // namespace

InputManager::InputManager(sf::RenderWindow& window, const common::KeyBindings& bindings)
    : bindings_(bindings), window_(window) {}

void InputManager::handleEvents() {
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            setKey(key->code, true);
        } else if (const auto* key = event->getIf<sf::Event::KeyReleased>()) {
            setKey(key->code, false);
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (spellReady_ && mouse->button == sf::Mouse::Button::Left) {
                clickedSpell_ = selectedSpell_;
                spellClick_ = mouse->position;
                spellClicked_ = true;
                spellReady_ = false;
                continue;
            }
            setMouseButton(mouse->button, true);
            if (edges_.lightPressed || edges_.heavyPressed)
                attackMousePosition_ = mouse->position;
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
            setMouseButton(mouse->button, false);
        } else if (event->is<sf::Event::FocusLost>()) {
            clearAll();
        }
    }
}

common::InputCommand InputManager::buildCommand(sf::Vector2f playerPosition, const sf::View& worldView,
                                                const common::CollisionWorld& walls) {
    common::InputCommand cmd;
    cmd.sequence = nextSequence_;
    cmd.spellKind = clickedSpell_;
    cmd.spellPressed = spellClicked_;
    if (spellClick_)
        cmd.spellTarget = window_.mapPixelToCoords(*spellClick_, worldView);
    if (cmd.spellPressed) {
        common::PlayerState caster;
        caster.pos = playerPosition;
        if (!common::spellTargetValid(caster, cmd.spellKind, cmd.spellTarget, walls, settings_)) {
            cmd.spellPressed = false;
            spellReady_ = true;
        }
    }
    spellClicked_ = false;
    spellClick_.reset();

    if (keys_.left)
        cmd.move.x -= 1.f;
    if (keys_.right)
        cmd.move.x += 1.f;
    if (keys_.up)
        cmd.move.y -= 1.f;
    if (keys_.down)
        cmd.move.y += 1.f;
    cmd.sprint = !keys_.walk && !keys_.rightWalk;

    normalize2D(cmd.move.x, cmd.move.y);
    const auto cursor = attackMousePosition_.value_or(sf::Mouse::getPosition(window_));
    cmd.aim = window_.mapPixelToCoords(cursor, worldView) - playerPosition;
    normalize2D(cmd.aim.x, cmd.aim.y);
    cmd.cursor = window_.mapPixelToCoords(sf::Mouse::getPosition(window_), worldView);
    cmd.hasCursor = window_.hasFocus();
    attackMousePosition_.reset();

    cmd.lightHeld = keys_.light;
    cmd.lightPressed = edges_.lightPressed;
    cmd.lightReleased = edges_.lightReleased;

    cmd.heavyHeld = keys_.heavy;
    cmd.heavyPressed = edges_.heavyPressed;
    cmd.heavyReleased = edges_.heavyReleased;

    // Consume edges only when a simulation command is produced.
    edges_ = {};

    seq_buffer.insert(nextSequence_, cmd);
    nextSequence_++;

    return cmd;
}

const InputManager::KeyState& InputManager::keys() const {
    return keys_;
}

void InputManager::clearAll() {
    lightningKeyHeld_ = false;
    spellReady_ = spellKeyHeld_ = spellClicked_ = false;
    spellClick_.reset();
    held_.clear();
    keys_ = {};
    debugKeyHeld_ = false;
    attackMousePosition_.reset();
    edges_ = {};
}

void InputManager::setKey(sf::Keyboard::Key key, bool pressed) {
    if (key != sf::Keyboard::Key::Unknown)
        setBinding(int(key), pressed);
}
void InputManager::setMouseButton(sf::Mouse::Button button, bool pressed) {
    setBinding(common::mouseBinding(button), pressed);
}
void InputManager::setBinding(int code, bool pressed) {
    if (pressed)
        held_.insert(code);
    else
        held_.erase(code);
    updateMovementBindings();
    updateAttackEdges();
    updateDebugToggle();
    updateSpellSelection();
}
bool InputManager::active(common::Action action) const {
    const auto& inputs = bindings_[action];
    return std::any_of(inputs.begin(), inputs.end(), [&](int input) { return held_.count(input) > 0; });
}
void InputManager::updateMovementBindings() {
    keys_.up = active(common::Action::MoveUp);
    keys_.down = active(common::Action::MoveDown);
    keys_.left = active(common::Action::MoveLeft);
    keys_.right = active(common::Action::MoveRight);
    keys_.walk = active(common::Action::Walk);
    keys_.rightWalk = false;
    keys_.scoreboard = active(common::Action::Scoreboard);
}
void InputManager::updateAttackEdges() {
    const bool light = active(common::Action::LightAttack), heavy = active(common::Action::HeavyAttack);
    edges_.lightPressed |= light && !keys_.light;
    edges_.lightReleased |= !light && keys_.light;
    edges_.heavyPressed |= heavy && !keys_.heavy;
    edges_.heavyReleased |= !heavy && keys_.heavy;
    if ((light && !keys_.light) || (heavy && !keys_.heavy))
        attackMousePosition_ = sf::Mouse::getPosition(window_);
    keys_.light = light;
    keys_.heavy = heavy;
}
void InputManager::updateDebugToggle() {
    const bool debug = active(common::Action::Debug);
    if (debug && !debugKeyHeld_)
        collisionDebugEnabled_ = !collisionDebugEnabled_;
    debugKeyHeld_ = debug;
}
void InputManager::updateSpellSelection() {
    const bool spell = active(common::Action::Explosion);
    if (spell && !spellKeyHeld_ && explosionAvailable_) {
        spellReady_ = !spellReady_ || selectedSpell_ != common::AttackKind::Explosion;
        selectedSpell_ = common::AttackKind::Explosion;
    }
    const bool lightning = active(common::Action::Lightning);
    if (lightning && !lightningKeyHeld_ && lightningAvailable_) {
        spellReady_ = !spellReady_ || selectedSpell_ != common::AttackKind::Lightning;
        selectedSpell_ = common::AttackKind::Lightning;
    }
    lightningKeyHeld_ = lightning;
    spellKeyHeld_ = spell;
}
