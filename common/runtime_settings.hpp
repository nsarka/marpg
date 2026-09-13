#pragma once
#include "settings.hpp"
namespace common {
inline constexpr const char* MSG_RUNTIME_SETTINGS = "runtime_settings";
inline constexpr const char* MSG_RUNTIME_ACK = "runtime_ack";
struct RuntimeSettings {
    std::uint32_t revision = 1, world = 1, botMask = 0;
};
inline void writeRuntime(sf::Packet& packet, const RuntimeSettings& runtime) {
    packet << runtime.revision << runtime.world << runtime.botMask;
}
inline bool readRuntime(sf::Packet& packet, RuntimeSettings& runtime) {
    return bool(packet >> runtime.revision >> runtime.world >> runtime.botMask);
}
} // namespace common
