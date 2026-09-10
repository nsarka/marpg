#include "common/character_visual.hpp"
#pragma once

#include "common/settings.hpp"
#include "nameplate.hpp"
#include "player_animation.hpp"
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
    using Facing8 = PlayerAnimation::Facing8;

    using Anim = common::CharacterAnimation;

    explicit Player(common::PlayerState initialState = {});

    void setSettings(const common::ServerSettings &settings) { settings_ = settings; }
    void setCharacterAnimations(const ResourceManager::CharacterAnimations &animations);

    void setFont(const sf::Font &font, unsigned int characterSize = 16);
    void setNameColor(sf::Color color);
    void setSpriteScale(sf::Vector2f scale);
    void setOriginToFeet(float xFraction = common::CharacterFeetX, float yFraction = common::CharacterFeetY);
    void setInterpolationSharpness(float sharpness);
    void setWalkSpeed(float speed);
    void setRunSpeed(float speed);

    void setTint(const sf::Color &color);

    void setOutlineEnabled(bool enabled);
    void setOutlineColor(const sf::Color &color);
    void setOutlineThickness(float pixels);
    void setOutlineShader(sf::Shader *shader);
    void setOcclusionShader(sf::Shader *shader) { m_occlusionShader = shader; }

    void setCombatIdle(bool value) { m_inCombatIdle = value; }
    void applySnapshot(const common::PlayerState &snapshot);
    void teleportTo(sf::Vector2f position);

    void update(float dtSeconds);

    void playOneShot(Anim anim);
    void setFacingFromVector(sf::Vector2f dir);

    common::PlayerState &state();
    sf::Vector2f renderPosition() const;
    Facing8 facing() const;
    Anim currentAnimation() const;
    bool isAlive() const;
    bool isConnected() const;

  private:
    void refreshCurrentFrame();
    void refreshOriginFromCurrentFrame();

    void updateNameTextPosition();

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

  private:
    common::ServerSettings settings_;
    common::PlayerState m_state{};

    PlayerAnimation animation_;
    Nameplate nameplate_;

    std::optional<sf::Sprite> m_sprite;

    sf::Vector2f m_renderPos{0.f, 0.f};
    sf::Vector2f m_targetPos{0.f, 0.f};

    float m_interpSharpness = 12.f;
    float m_walkSpeed = 50.f;
    float m_runSpeed = 140.f;

    bool m_inCombatIdle = false;
    float m_originXF = common::CharacterFeetX;
    // Fantasy sheets use a stable foot pivot at (64,88) in each 128px cell.
    float m_originYF = common::CharacterFeetY;

    bool m_outlineEnabled = false;
    sf::Color m_outlineColor = sf::Color(255, 60, 60, 220);
    float m_outlineThickness = 2.f;
    sf::Shader *m_occlusionShader = nullptr; // non-owning
    sf::Shader *m_outlineShader = nullptr;   // non-owning
};
