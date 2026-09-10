#pragma once
#include "common/animation_catalog.hpp"
#include "common/common.hpp"
#include "common/keybindings.hpp"
#include "sound_types.hpp"
#include <array>
#include <stdexcept>
namespace client {
enum class SpellVisual { None, Explosion, Lightning };
struct AttackPresentation {
    const char* name;
    common::CharacterAnimation animation;
    common::Action bindingAction;
    std::size_t impactFrame;
    SoundEffect castSound;
    SoundEffect impactSound;
    SpellVisual effect;
};
inline const AttackPresentation& attackPresentation(common::AttackKind kind) {
    using A = common::CharacterAnimation;
    static constexpr std::array<AttackPresentation, 4> definitions{
        {{"LIGHT", A::Attack1, common::Action::LightAttack, 7, SoundEffect::Swing, SoundEffect::Punch,
          SpellVisual::None},
         {"HEAVY", A::Attack4, common::Action::HeavyAttack, 8, SoundEffect::Swing, SoundEffect::Punch,
          SpellVisual::None},
         {"EXPLOSION", A::Special1, common::Action::Explosion, 10, SoundEffect::Cast, SoundEffect::Blast,
          SpellVisual::Explosion},
         {"LIGHTNING", A::Special1, common::Action::Lightning, 10, SoundEffect::Cast, SoundEffect::Blast,
          SpellVisual::Lightning}}};
    return definitions.at(static_cast<unsigned>(kind) - 1);
}
} // namespace client
