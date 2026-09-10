#include "settings.hpp"
#include <algorithm>
namespace common {
AttackDesc attackDescription(AttackKind kind, const ServerSettings &settings) {
    const auto ticks = [](double seconds) {
        return std::max(1u, static_cast<Tick>(std::llround(seconds * TICK_RATE)));
    };
    if (kind == AttackKind::Explosion || kind == AttackKind::Lightning) {
        const auto &spell = kind == AttackKind::Lightning ? settings.lightning : settings.spell;
        return {ticks(spell.windup),   kind == AttackKind::Lightning ? ticks(spell.duration) : 1,
                ticks(spell.cooldown), float(spell.radius),
                spell.damageMax,       spell.damageMin,
                float(spell.range),    360.f,
                spell.interval,        true};
    }
    if (kind == AttackKind::Light || kind == AttackKind::Heavy) {
        const bool light = kind == AttackKind::Light;
        const float range = float(light ? settings.lightRange : settings.heavyRange);
        return {ticks(light ? settings.lightWindup : settings.heavyWindup),
                8,
                light ? 40u : 32u,
                range,
                light ? settings.lightDamage : settings.heavyDamage,
                light ? settings.lightDamageMin : settings.heavyDamageMin,
                range,
                float(light ? settings.lightConeDegrees : settings.heavyConeDegrees),
                0,
                false};
    }
    throw std::invalid_argument("No definition for this attack");
}
} // namespace common
