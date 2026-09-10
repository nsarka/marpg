#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace common {
enum class CharacterAnimation : std::uint8_t {
    Attack1, Attack2, Attack3, Attack4, Attack5, AttackRun, AttackRun2, CrouchIdle, CrouchRun, Die, Idle, Idle2, Idle3, Idle4, RideIdle, RideRun, Run, RunBackwards, Special1, StrafeLeft, StrafeRight, TakeDamage, Taunt, Walk, Count
};
struct AnimationDefinition {const char* name; bool looping; float frameSeconds;};
inline constexpr std::array<AnimationDefinition,static_cast<std::size_t>(CharacterAnimation::Count)> CharacterAnimations{{
    {"Attack1",false,.05f},
    {"Attack2",false,.05f},
    {"Attack3",false,.05f},
    {"Attack4",false,.05f},
    {"Attack5",false,.05f},
    {"AttackRun",false,.05f},
    {"AttackRun2",false,.05f},
    {"CrouchIdle",true,.05f},
    {"CrouchRun",true,.05f},
    {"Die",false,.065f},
    {"Idle",true,.05f},
    {"Idle2",true,.05f},
    {"Idle3",true,.05f},
    {"Idle4",true,.05f},
    {"RideIdle",true,.05f},
    {"RideRun",true,.05f},
    {"Run",true,.05f},
    {"RunBackwards",true,.05f},
    {"Special1",false,.05f},
    {"StrafeLeft",true,.05f},
    {"StrafeRight",true,.05f},
    {"TakeDamage",false,.025f},
    {"Taunt",false,.05f},
    {"Walk",true,.05f},
}};
}
