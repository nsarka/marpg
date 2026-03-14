#include "player.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

Player::Player(common::PlayerState initialState)
    : m_state(std::move(initialState))
    , m_renderPos(m_state.pos)
    , m_targetPos(m_state.pos)
{
}

void Player::setCharacterAnimations(const ResourceManager::CharacterAnimations& animations)
{
    m_anims = &animations;
    setAnimation(Anim::Idle, true);
}

float Player::lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

Player::Facing8 Player::vectorToFacing8(sf::Vector2f dir)
{
    const float pi = 3.14159265358979323846f;
    const float angle = std::atan2(dir.y, dir.x);
    const float normalized = (angle + pi) / (2.f * pi);
    const int sector = static_cast<int>(std::floor(normalized * 8.f + 0.5f)) % 8;

    switch (sector) {
        case 0: return Facing8::Dir2;
        case 1: return Facing8::Dir3;
        case 2: return Facing8::Dir4;
        case 3: return Facing8::Dir5;
        case 4: return Facing8::Dir6;
        case 5: return Facing8::Dir7;
        case 6: return Facing8::Dir8;
        case 7: return Facing8::Dir1;
        default: return Facing8::Dir1;
    }
}

const ResourceManager::Clip& Player::currentClip()
{
    if (!m_anims) {
        throw std::runtime_error("Player animations not set");
    }

    auto& set = m_anims->anims[animIndex(m_currentAnim)];
    if (!set.loaded) {
        throw std::runtime_error("Animation set not loaded");
    }

    return set.byFacing[facingIndex(m_facing)];
}

const ResourceManager::Clip& Player::currentClip() const
{
    if (!m_anims) {
        throw std::runtime_error("Player animations not set");
    }

    const auto& set = m_anims->anims[animIndex(m_currentAnim)];
    if (!set.loaded) {
        throw std::runtime_error("Animation set not loaded");
    }

    return set.byFacing[facingIndex(m_facing)];
}

void Player::setAnimation(Anim anim, bool restart)
{
    if (!restart && m_currentAnim == anim) {
        refreshCurrentFrame();
        return;
    }

    m_currentAnim = anim;
    m_frameIndex = 0;
    m_frameTime = 0.f;
    refreshCurrentFrame();
}

void Player::refreshOriginFromCurrentFrame()
{
    if (!m_sprite) {
        return;
    }

    const sf::IntRect rect = m_sprite->getTextureRect();
    const float ox = static_cast<float>(rect.size.x) * m_originXF;
    const float oy = static_cast<float>(rect.size.y) * m_originYF;
    m_sprite->setOrigin({ox, oy});
}

void Player::refreshCurrentFrame()
{
    const ResourceManager::Clip& clip = currentClip();
    if (clip.frames.empty() || !clip.texture) {
        return;
    }

    if (m_frameIndex >= clip.frames.size()) {
        m_frameIndex = clip.frames.size() - 1;
    }

    if (!m_sprite.has_value()) {
        m_sprite.emplace(*clip.texture, clip.frames[m_frameIndex].rect);
    } else {
        m_sprite->setTexture(*clip.texture, true);
        m_sprite->setTextureRect(clip.frames[m_frameIndex].rect);
    }

    refreshOriginFromCurrentFrame();
    m_sprite->setPosition(m_renderPos);
    updateNameTextPosition();
}

void Player::stepAnimation(float dtSeconds)
{
    const ResourceManager::Clip& clip = currentClip();
    if (clip.frames.empty()) {
        return;
    }

    m_frameTime += dtSeconds;

    while (m_frameTime >= clip.frames[m_frameIndex].durationSeconds) {
        m_frameTime -= clip.frames[m_frameIndex].durationSeconds;
        ++m_frameIndex;

        if (m_frameIndex >= clip.frames.size()) {
            if (clip.looping) {
                m_frameIndex = 0;
            } else {
                m_frameIndex = clip.frames.size() - 1;

                if (m_lockedAnim && *m_lockedAnim == m_currentAnim) {
                    m_lockedAnim.reset();
                }

                break;
            }
        }
    }

    refreshCurrentFrame();
}

void Player::setFont(const sf::Font& font, unsigned int characterSize)
{
    m_nameText.emplace(font, m_state.name, characterSize);
    m_nameText->setFillColor(sf::Color::White);
    centerNameText();
}

void Player::setNameColor(sf::Color color)
{
    if (m_nameText) {
        m_nameText->setFillColor(color);
    }
}

void Player::setSpriteScale(sf::Vector2f scale)
{
    if (m_sprite) {
        m_sprite->setScale(scale);
        updateNameTextPosition();
    }
}

void Player::setOriginToFeet(float xFraction, float yFraction)
{
    m_originXF = xFraction;
    m_originYF = yFraction;
    refreshOriginFromCurrentFrame();
    updateNameTextPosition();
}

void Player::setInterpolationSharpness(float sharpness)
{
    m_interpSharpness = sharpness;
}

void Player::setWalkSpeed(float speed)
{
    m_walkSpeed = speed;
}

void Player::setRunSpeed(float speed)
{
    m_runSpeed = speed;
}

void Player::setAirborne(bool value)
{
    m_isAirborne = value;
}

void Player::setBlocking(bool value)
{
    m_isBlocking = value;
}

void Player::setCarrying(bool value)
{
    m_isCarrying = value;
}

void Player::setCombatIdle(bool value)
{
    m_inCombatIdle = value;
}

void Player::setTint(const sf::Color& color)
{
    if (m_sprite) {
        m_sprite->setColor(color);
    }
}

void Player::setOutlineEnabled(bool enabled)
{
    m_outlineEnabled = enabled;
}

void Player::setOutlineColor(const sf::Color& color)
{
    m_outlineColor = color;
}

void Player::setOutlineThickness(float pixels)
{
    m_outlineThickness = pixels;
}

void Player::setOutlineShader(sf::Shader* shader)
{
    m_outlineShader = shader;
}

void Player::applySnapshot(const common::PlayerState& snapshot)
{
    m_state = snapshot;
    m_targetPos = snapshot.pos;

    if (m_nameText) {
        m_nameText->setString(m_state.name);
        centerNameText();
    }

    if (!m_state.alive && m_currentAnim != Anim::Die) {
        playOneShot(Anim::Die);
    }
}

void Player::teleportTo(sf::Vector2f position)
{
    m_state.pos = position;
    m_targetPos = position;
    m_renderPos = position;

    if (m_sprite) {
        m_sprite->setPosition(position);
    }

    updateNameTextPosition();
}

void Player::playOneShot(Anim anim)
{
    m_lockedAnim = anim;
    setAnimation(anim, true);
}

void Player::setFacingFromVector(sf::Vector2f dir)
{
    if (lengthSquared(dir) < 0.0001f) {
        return;
    }

    m_facing = vectorToFacing8(dir);
    refreshCurrentFrame();
}

void Player::update(float dtSeconds)
{
    if (!m_anims) {
        return;
    }

    const float alpha = 1.f - std::exp(-m_interpSharpness * dtSeconds);
    m_renderPos += (m_targetPos - m_renderPos) * alpha;

    if (m_sprite) {
        m_sprite->setPosition(m_renderPos);
    }
    updateNameTextPosition();

    if (lengthSquared(m_state.vel) > 0.0001f) {
        m_facing = vectorToFacing8(m_state.vel);
    }

    if (!m_lockedAnim.has_value()) {
        const float speed2 = lengthSquared(m_state.vel);

        if (!m_state.alive) {
            if (m_currentAnim != Anim::Die) {
                setAnimation(Anim::Die, true);
            }
        } else if (m_isBlocking) {
            if (m_currentAnim != Anim::Block) {
                setAnimation(Anim::Block, false);
            }
        } else if (m_inCombatIdle && speed2 < m_walkSpeed * m_walkSpeed) {
            if (m_currentAnim != Anim::FightIdle) {
                setAnimation(Anim::FightIdle, false);
            }
        } else if (m_isCarrying) {
            if (m_currentAnim != Anim::Carrying) {
                setAnimation(Anim::Carrying, false);
            }
        } else if (m_isAirborne) {
            const Anim desired = (speed2 > m_runSpeed * m_runSpeed)
                ? Anim::RunningJump
                : Anim::Jump;
            if (m_currentAnim != desired) {
                setAnimation(desired, false);
            }
        } else if (speed2 >= m_runSpeed * m_runSpeed) {
            if (m_currentAnim != Anim::Running) {
                setAnimation(Anim::Running, false);
            }
        } else if (speed2 >= m_walkSpeed * m_walkSpeed) {
            if (m_currentAnim != Anim::Walk) {
                setAnimation(Anim::Walk, false);
            }
        } else {
            if (m_currentAnim != Anim::Idle) {
                setAnimation(Anim::Idle, false);
            }
        }
    }

    if (m_state.vel.x != 0.f || m_state.vel.y != 0.f) {
        setFacingFromVector(m_state.vel);
    }

    stepAnimation(dtSeconds);
}

bool Player::isAlive() const
{
    return m_state.alive;
}

bool Player::isConnected() const
{
    return m_state.connected;
}

common::PlayerState& Player::state()
{
    return m_state;
}

sf::Vector2f Player::renderPosition() const
{
    return m_renderPos;
}

Player::Facing8 Player::facing() const
{
    return m_facing;
}

Player::Anim Player::currentAnimation() const
{
    return m_currentAnim;
}

void Player::updateNameTextPosition()
{
    if (!m_nameText) {
        return;
    }

    const sf::FloatRect textBounds = m_nameText->getLocalBounds();

    float spriteTopY = m_renderPos.y - 80.f;
    if (m_sprite) {
        spriteTopY = m_sprite->getGlobalBounds().position.y;
    }

    m_nameText->setPosition({
        m_renderPos.x - textBounds.size.x * 0.5f,
        spriteTopY + 45.f
    });
}

void Player::centerNameText()
{
    updateNameTextPosition();
}

void Player::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if (!m_state.connected) {
        return;
    }

    if (m_sprite) {
        if (m_outlineEnabled && m_outlineShader) {
            sf::RenderStates outlineStates = states;
            outlineStates.shader = m_outlineShader;

            const sf::Texture& tex = m_sprite->getTexture();
            const auto size = tex.getSize();

            m_outlineShader->setUniform("texture", sf::Shader::CurrentTexture);
            m_outlineShader->setUniform("texelSize",
                                        sf::Glsl::Vec2(
                                            1.f / static_cast<float>(size.x),
                                            1.f / static_cast<float>(size.y)));
            m_outlineShader->setUniform("thickness", m_outlineThickness);
            m_outlineShader->setUniform("outlineColor",
                                        sf::Glsl::Vec4(
                                            m_outlineColor.r / 255.f,
                                            m_outlineColor.g / 255.f,
                                            m_outlineColor.b / 255.f,
                                            m_outlineColor.a / 255.f));

            target.draw(*m_sprite, outlineStates);
        }

        target.draw(*m_sprite, states);
    }

    if (m_nameText) {
        target.draw(*m_nameText, states);
    }
}
