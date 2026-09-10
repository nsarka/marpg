#include "player.hpp"
#include "common/settings.hpp"
#include "melee_animation.hpp"
#include "ui_font.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <algorithm>

#include <cmath>
#include <stdexcept>
#include <utility>

Player::Player(common::PlayerState initialState)
    : m_state(std::move(initialState)), m_renderPos(m_state.pos), m_targetPos(m_state.pos) {}

void Player::setCharacterAnimations(const ResourceManager::CharacterAnimations &animations) {
    animation_.setAnimations(animations);
    refreshCurrentFrame();
}

void Player::refreshOriginFromCurrentFrame() {
    if (!m_sprite) {
        return;
    }

    const sf::IntRect rect = m_sprite->getTextureRect();
    const float ox = static_cast<float>(rect.size.x) * m_originXF;
    const float oy = static_cast<float>(rect.size.y) * m_originYF;
    m_sprite->setOrigin({ox, oy});
}

void Player::refreshCurrentFrame() {
    if (!animation_.ready())
        return;
    const ResourceManager::Clip &clip = animation_.clip();
    if (clip.frames.empty() || !clip.texture) {
        return;
    }

    const auto frameIndex = std::min(animation_.frame(), clip.frames.size() - 1);

    if (!m_sprite.has_value()) {
        m_sprite.emplace(*clip.texture, clip.frames[frameIndex].rect);
    } else {
        m_sprite->setTexture(*clip.texture, true);
        m_sprite->setTextureRect(clip.frames[frameIndex].rect);
    }

    refreshOriginFromCurrentFrame();
    m_sprite->setPosition(m_renderPos);
    updateNameTextPosition();
}

void Player::setFont(const sf::Font &font, unsigned int characterSize) {
    nameplate_.setFont(font, characterSize);
    updateNameTextPosition();
}

void Player::setNameColor(sf::Color color) { nameplate_.setColor(color); }

void Player::setSpriteScale(sf::Vector2f scale) {
    if (m_sprite) {
        m_sprite->setScale(scale);
        updateNameTextPosition();
    }
}

void Player::setOriginToFeet(float xFraction, float yFraction) {
    m_originXF = xFraction;
    m_originYF = yFraction;
    refreshOriginFromCurrentFrame();
    updateNameTextPosition();
}

void Player::setInterpolationSharpness(float sharpness) { m_interpSharpness = sharpness; }

void Player::setWalkSpeed(float speed) { m_walkSpeed = speed; }

void Player::setRunSpeed(float speed) { m_runSpeed = speed; }

void Player::setTint(const sf::Color &color) {
    if (m_sprite) {
        m_sprite->setColor(color);
    }
}

void Player::setOutlineEnabled(bool enabled) { m_outlineEnabled = enabled; }

void Player::setOutlineColor(const sf::Color &color) {
    m_outlineColor = color;
    m_outlineColor.a = static_cast<std::uint8_t>(color.a * 0.2f);
}

void Player::setOutlineThickness(float pixels) { m_outlineThickness = pixels; }

void Player::setOutlineShader(sf::Shader *shader) { m_outlineShader = shader; }

void Player::applySnapshot(const common::PlayerState &snapshot) {
    const bool teleported = snapshot.teleportSequence != m_state.teleportSequence;
    const bool respawned = (!m_state.connected || !m_state.alive) && snapshot.alive;
    animation_.observe(m_state, snapshot);
    m_state = snapshot;
    m_targetPos = snapshot.pos;
    if (teleported || respawned)
        teleportTo(snapshot.pos);
    refreshCurrentFrame();
    updateNameTextPosition();
}

void Player::teleportTo(sf::Vector2f position) {
    m_state.pos = position;
    m_targetPos = position;
    m_renderPos = position;

    if (m_sprite) {
        m_sprite->setPosition(position);
    }

    updateNameTextPosition();
}

void Player::playOneShot(Anim anim) {
    animation_.oneShot(anim);
    refreshCurrentFrame();
}

void Player::setFacingFromVector(sf::Vector2f dir) {
    animation_.face(dir);
    refreshCurrentFrame();
}

void Player::update(float dtSeconds) {
    if (!animation_.ready())
        return;
    const float alpha = 1.f - std::exp(-m_interpSharpness * dtSeconds);
    m_renderPos += (m_targetPos - m_renderPos) * alpha;
    animation_.update(dtSeconds, m_state, settings_, m_inCombatIdle, m_walkSpeed, m_runSpeed);
    refreshCurrentFrame();
    updateNameTextPosition();
}

bool Player::isAlive() const { return m_state.alive; }

bool Player::isConnected() const { return m_state.connected; }

common::PlayerState &Player::state() { return m_state; }

sf::Vector2f Player::renderPosition() const { return m_renderPos; }

Player::Facing8 Player::facing() const { return animation_.facing(); }

Player::Anim Player::currentAnimation() const { return animation_.current(); }

void Player::updateNameTextPosition() { nameplate_.update(m_state.name, m_renderPos); }

void Player::draw(sf::RenderTarget &target, sf::RenderStates states) const {
    if (!m_state.connected) {
        return;
    }

    if (m_sprite) {
        if (m_occlusionShader) {
            const auto size = m_sprite->getTexture().getSize();
            m_occlusionShader->setUniform("texture", sf::Shader::CurrentTexture);
            m_occlusionShader->setUniform("footDepth", m_renderPos.y);
            m_occlusionShader->setUniform("texelSize", sf::Glsl::Vec2(1.f / size.x, 1.f / size.y));
            m_occlusionShader->setUniform("outlined", m_outlineEnabled);
            m_occlusionShader->setUniform("thickness", m_outlineThickness);
            m_occlusionShader->setUniform("outlineColor", sf::Glsl::Vec4(m_outlineColor));
            states.shader = m_occlusionShader;
        } else if (m_outlineEnabled && m_outlineShader) {
            sf::RenderStates outlineStates = states;
            outlineStates.shader = m_outlineShader;

            const sf::Texture &tex = m_sprite->getTexture();
            const auto size = tex.getSize();

            m_outlineShader->setUniform("texture", sf::Shader::CurrentTexture);
            m_outlineShader->setUniform("texelSize", sf::Glsl::Vec2(1.f / static_cast<float>(size.x),
                                                                    1.f / static_cast<float>(size.y)));
            m_outlineShader->setUniform("thickness", m_outlineThickness);
            m_outlineShader->setUniform("outlineColor",
                                        sf::Glsl::Vec4(m_outlineColor.r / 255.f, m_outlineColor.g / 255.f,
                                                       m_outlineColor.b / 255.f, m_outlineColor.a / 255.f));

            target.draw(*m_sprite, outlineStates);
        }

        target.draw(*m_sprite, states);
    }

    nameplate_.draw(target, states, m_state, m_renderPos, settings_.teams);
}
