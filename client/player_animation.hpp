#pragma once
#include "attack_presentation.hpp"
#include "common/settings.hpp"
#include "melee_animation.hpp"
#include "resource_manager.hpp"
#include <optional>

class PlayerAnimation {
  public:
    enum class Facing8 : std::uint8_t { Dir1, Dir2, Dir3, Dir4, Dir5, Dir6, Dir7, Dir8 };
    using Anim = common::CharacterAnimation;
    void setAnimations(const ResourceManager::CharacterAnimations& animations) {
        animations_ = &animations;
        select(Anim::Idle, true);
    }
    bool ready() const {
        return animations_ != nullptr;
    }
    Facing8 facing() const {
        return facing_;
    }
    Anim current() const {
        return current_;
    }
    std::size_t frame() const {
        return frame_;
    }
    const ResourceManager::Clip& clip() const {
        if (!animations_)
            throw std::runtime_error("Player animations not set");
        return animations_->get(current_).byFacing[static_cast<std::size_t>(facing_)];
    }
    void select(Anim animation, bool restart = false) {
        if (!restart && current_ == animation)
            return;
        current_ = animation;
        frame_ = 0;
        elapsed_ = 0;
    }
    void oneShot(Anim animation) {
        locked_ = animation;
        select(animation, true);
    }
    void face(sf::Vector2f direction) {
        if (direction.lengthSquared() < .0001f)
            return;
        constexpr float pi = 3.14159265358979323846f;
        const float normalized = (std::atan2(direction.y, direction.x) + pi) / (2.f * pi);
        const int sector = static_cast<int>(std::floor(normalized * 8.f + .5f)) % 8;
        facing_ = static_cast<Facing8>((sector + 1) % 8);
    }
    void observe(const common::PlayerState& previous, const common::PlayerState& state) {
        if ((!previous.connected || !previous.alive) && state.alive) {
            locked_.reset();
            facing_ = Facing8::Dir1;
            select(Anim::Idle, true);
        }
        if (state.attackSequence != previous.attackSequence && state.alive &&
            state.lastAttack != common::AttackKind::None) {
            face(state.facing);
            const auto animation = client::attackPresentation(state.lastAttack).animation;
            if (state.lastAttack == common::AttackKind::Lightning)
                select(animation, true);
            else
                oneShot(animation);
        }
        if (previous.connected && state.health < previous.health && state.alive)
            oneShot(Anim::TakeDamage);
        if (!state.alive && current_ != Anim::Die)
            oneShot(Anim::Die);
    }
    void update(float dt, const common::PlayerState& state, const common::ServerSettings& settings,
                bool combatIdle, float walkSpeed, float runSpeed) {
        if (!ready())
            return;
        face(state.facing);
        const auto lightning = common::attackDescription(common::AttackKind::Lightning, settings);
        const bool channeling =
            state.alive && state.combatDebug.attack == common::AttackKind::Lightning &&
            state.combatDebug.elapsedTicks < lightning.startupTicks + lightning.activeTicks;
        if (channeling && !locked_)
            select(Anim::Special1);
        if (!locked_ && !channeling) {
            const float speed = state.vel.lengthSquared();
            Anim locomotion = Anim::Idle;
            if (!state.alive)
                locomotion = Anim::Die;
            else if (combatIdle && speed < walkSpeed * walkSpeed)
                locomotion = Anim::Idle2;
            else if (speed >= runSpeed * runSpeed)
                locomotion = Anim::Run;
            else if (speed >= walkSpeed * walkSpeed)
                locomotion = Anim::Walk;
            select(locomotion);
        }
        const auto& frames = clip().frames;
        if (frames.empty())
            return;
        elapsed_ += dt;
        const auto duration = [&]() {
            if (current_ == Anim::Attack1 || current_ == Anim::Attack4)
                return client::meleeFrameDuration(current_ == Anim::Attack1 ? common::AttackKind::Light
                                                                            : common::AttackKind::Heavy,
                                                  frames, frame_, settings);
            if (current_ == Anim::Special1 && frame_ < 10) {
                const auto kind = state.lastAttack == common::AttackKind::Lightning
                                      ? common::AttackKind::Lightning
                                      : common::AttackKind::Explosion;
                return common::attackDescription(kind, settings).startupTicks * common::TICK_DT / 10.f;
            }
            return frames[frame_].durationSeconds;
        };
        while (elapsed_ >= duration()) {
            elapsed_ -= duration();
            ++frame_;
            if (frame_ >= frames.size()) {
                if (clip().looping ||
                    (current_ == Anim::Special1 && state.combatDebug.attack == common::AttackKind::Lightning))
                    frame_ = 0;
                else {
                    frame_ = frames.size() - 1;
                    if (locked_ && *locked_ == current_)
                        locked_.reset();
                    break;
                }
            }
        }
    }

  private:
    const ResourceManager::CharacterAnimations* animations_ = nullptr;
    Facing8 facing_ = Facing8::Dir1;
    Anim current_ = Anim::Idle;
    std::optional<Anim> locked_;
    std::size_t frame_ = 0;
    float elapsed_ = 0;
};
