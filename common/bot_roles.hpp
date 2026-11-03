#pragma once
#include "common.hpp"

namespace common {
enum class BotRole { Bruiser, Caster, Charger };
inline constexpr BotRole botRole(PlayerId id) {
    return static_cast<BotRole>(id % 3);
}
inline constexpr const char* botRoleName(PlayerId id) {
    switch (botRole(id)) {
    case BotRole::Bruiser:
        return "Bruiser";
    case BotRole::Caster:
        return "Caster";
    case BotRole::Charger:
        return "Charger";
    }
    return "Bot";
}
inline constexpr std::uint8_t botCharacter(PlayerId id) {
    // Enemy 1/2/3 remain recognizable across teams; outlines show allegiance.
    return static_cast<std::uint8_t>(botRole(id));
}
inline std::string botName(PlayerId id) {
    return "Bot " + std::to_string(id + 1);
}
} // namespace common
