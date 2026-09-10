#include "settings.hpp"
#include <algorithm>
namespace common {
AttackDefinition attackDescription(AttackKind kind, const ServerSettings& settings) {
    const auto ticks = [](double seconds) {
        return std::max(1u, static_cast<Tick>(std::llround(seconds * TICK_RATE)));
    };
    if (isSpell(kind)) {
        const auto& spell = kind == AttackKind::Lightning ? settings.lightning : settings.spell;
        return {ticks(spell.windupSeconds),
                kind == AttackKind::Lightning ? ticks(spell.durationSeconds) : 1,
                ticks(spell.cooldownSeconds),
                float(spell.effectRadius),
                spell.damageMax,
                spell.damageMin,
                float(spell.castRange),
                360.f,
                spell.pulseIntervalSeconds,
                true};
    }
    if (kind == AttackKind::Light || kind == AttackKind::Heavy) {
        const bool light = kind == AttackKind::Light;
        const float range = float(light ? settings.lightRange : settings.heavyRange);
        return {ticks(light ? settings.lightWindupSeconds : settings.heavyWindupSeconds),
                8,
                light ? 40u : 32u,
                range,
                light ? settings.lightDamageMax : settings.heavyDamageMax,
                light ? settings.lightDamageMin : settings.heavyDamageMin,
                range,
                float(light ? settings.lightConeDegrees : settings.heavyConeDegrees),
                0,
                false};
    }
    throw std::invalid_argument("No definition for this attack");
}
} // namespace common
