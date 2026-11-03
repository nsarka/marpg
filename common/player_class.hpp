#pragma once
#include "common.hpp"
#include <array>

namespace common {
struct ClassDefinition {
    const char* name;
    const char* perk;
};
inline constexpr std::array<ClassDefinition, 4> PlayerClasses{{{"Adventurer", "Default stats"},
                                                               {"Bruiser", "+25% melee"},
                                                               {"Caster", "+25% spells"},
                                                               {"Charger", "+25% speed"}}};
inline const ClassDefinition& classDefinition(PlayerClass value) {
    return PlayerClasses.at(static_cast<std::size_t>(value));
}
inline float classMovementMultiplier(PlayerClass value) {
    return value == PlayerClass::Charger ? 1.25f : 1.f;
}
inline int classAttackDamage(PlayerClass value, AttackKind attack, int damage) {
    const bool bonus = (value == PlayerClass::Bruiser && !isSpell(attack)) ||
                       (value == PlayerClass::Caster && isSpell(attack));
    return bonus ? (damage * 5 + 2) / 4 : damage;
}
} // namespace common
