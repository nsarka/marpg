#pragma once

#include "common/common.hpp"
#include "resource_manager.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

class Player : public sf::Drawable {
public:
    enum class Facing8 : std::uint8_t {
        Dir1 = 0,
        Dir2,
        Dir3,
        Dir4,
        Dir5,
        Dir6,
        Dir7,
        Dir8
    };

    enum class Anim : std::uint8_t {
        Idle = 0,
        Walk,
        Running,
        Jump,
        RunningJump,
        RunningRoll,
        FightIdle,
        Block,
        LeftJab,
        RightHook,
        Uppercut,
        Combo,
        Damaged,
        Die,
        PickUp,
        PickUpLow,
        Carrying,
        Pull,
        Push,
        Talking,
        Count
    };

    explicit Player(common::PlayerState initialState = {});

    void setCharacterAnimations(const ResourceManager::CharacterAnimations& animations);

    void setFont(const sf::Font& font, unsigned int characterSize = 16);
    void setNameColor(sf::Color color);
    void setSpriteScale(sf::Vector2f scale);
    void setOriginToFeet(float xFraction = 0.5f, float yFraction = 0.6875f);
    void setInterpolationSharpness(float sharpness);
    void setWalkSpeed(float speed);
    void setRunSpeed(float speed);

    void setTint(const sf::Color& color);

    void setOutlineEnabled(bool enabled);
    void setOutlineColor(const sf::Color& color);
    void setOutlineThickness(float pixels);
    void setOutlineShader(sf::Shader* shader);
    void setOcclusionShader(sf::Shader* shader) { m_occlusionShader = shader; }

    void setAirborne(bool value);
    void setBlocking(bool value);
    void setCarrying(bool value);
    void setCombatIdle(bool value);

    void applySnapshot(const common::PlayerState& snapshot);
    void teleportTo(sf::Vector2f position);

    void update(float dtSeconds);

    void playOneShot(Anim anim);
    void setFacingFromVector(sf::Vector2f dir);

    common::PlayerState& state();
    sf::Vector2f renderPosition() const;
    Facing8 facing() const;
    Anim currentAnimation() const;
    bool isAlive() const;
    bool isConnected() const;

private:
    static constexpr std::size_t kAnimCount =
        static_cast<std::size_t>(Anim::Count);

    static constexpr std::size_t animIndex(Anim anim) {
        return static_cast<std::size_t>(anim);
    }

    static constexpr std::size_t facingIndex(Facing8 facing) {
        return static_cast<std::size_t>(facing);
    }

    static float lengthSquared(sf::Vector2f v);
    static Facing8 vectorToFacing8(sf::Vector2f dir);

    const ResourceManager::Clip& currentClip() const;
    const ResourceManager::Clip& currentClip();

    void setAnimation(Anim anim, bool restart);
    void refreshCurrentFrame();
    void refreshOriginFromCurrentFrame();
    void stepAnimation(float dtSeconds);

    void updateNameTextPosition();
    void centerNameText();

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    common::PlayerState m_state{};

    const ResourceManager::CharacterAnimations* m_anims = nullptr;

    std::optional<sf::Sprite> m_sprite;
    std::optional<sf::Text> m_nameText;

    sf::Vector2f m_renderPos{0.f, 0.f};
    sf::Vector2f m_targetPos{0.f, 0.f};

    Facing8 m_facing = Facing8::Dir1;
    Anim m_currentAnim = Anim::Idle;
    std::optional<Anim> m_lockedAnim;

    std::size_t m_frameIndex = 0;
    float m_frameTime = 0.f;

    float m_interpSharpness = 12.f;
    float m_walkSpeed = 50.f;
    float m_runSpeed = 140.f;

    bool m_isAirborne = false;
    bool m_isBlocking = false;
    bool m_isCarrying = false;
    bool m_inCombatIdle = false;

    float m_originXF = 0.5f;
    // Fantasy sheets use a stable foot pivot at (64,88) in each 128px cell.
    float m_originYF = 0.6875f;

    bool m_outlineEnabled = false;
    sf::Color m_outlineColor = sf::Color(255, 60, 60, 220);
    float m_outlineThickness = 2.f;
    sf::Shader* m_occlusionShader = nullptr; // non-owning
    sf::Shader* m_outlineShader = nullptr; // non-owning
};
