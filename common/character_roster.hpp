#pragma once
#include <array>
#include <cstdint>
#include <random>
namespace common {
inline constexpr std::array<const char*, 7> CharacterNames{"Enemy 1", "Enemy 2", "Enemy 3", "NPC1",
                                                           "NPC2",    "NPC3",    "Player"};
inline constexpr std::uint8_t DefaultCharacter = 6;
template <class Random> std::uint8_t chooseCharacter(int team, Random& random) {
    // Team 0 is red, team 1 blue. Additional team colors use the full roster.
    return static_cast<std::uint8_t>(
        std::uniform_int_distribution<int>(team == 1 ? 3 : 0, team == 0 ? 2 : 6)(random));
}
} // namespace common
