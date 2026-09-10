#pragma once
#include "attack_presentation.hpp"
#include "common/attack_delivery.hpp"
#include "common/common.hpp"
#include "sound_types.hpp"
#include <array>
#include <optional>
#include <string>

// Keep snapshot deduplication independent of playback and audio hardware.
struct SoundCue {
    SoundEffect effect;
    sf::Vector2f position;
};
inline std::optional<SoundEffect> soundCategory(const std::string& name) {
    const auto starts = [&](const char* prefix) { return name.rfind(prefix, 0) == 0; };
    if (starts("Jump_") && name.find("_Medium.wav") != std::string::npos)
        return SoundEffect::Teleport;
    if (starts("Charge_") && name.find("Up") != std::string::npos)
        return SoundEffect::Cast;
    if (starts("Impact_Near_"))
        return SoundEffect::Blast;
    if (starts("SwordSwing_"))
        return SoundEffect::Swing;
    if (starts("Punch_"))
        return SoundEffect::Punch;
    if (starts("Damage_Short_"))
        return SoundEffect::SmallDamage;
    if (starts("Big_Damage_"))
        return SoundEffect::Death;
    if (starts("SpecialFX_Magic_"))
        return SoundEffect::Respawn;
    return std::nullopt;
}
class SoundEvents {
  public:
    std::vector<SoundCue> observe(const std::vector<common::PlayerState>& players) {
        std::vector<SoundCue> cues;
        for (std::size_t i = 0; i < players.size() && i < seen_.size(); ++i) {
            const auto& player = players[i];
            auto& seen = seen_[i];
            if (!player.connected) {
                seen = {};
                continue;
            }
            // Joining a game establishes a baseline, never replays old combat history.
            if (!seen.initialized) {
                seen = {true,
                        player.alive,
                        player.attackSequence,
                        player.damageSequence,
                        player.spellSequence,
                        player.teleportSequence};
                continue;
            }
            if (common::sequenceNewer(player.teleportSequence, seen.teleport)) {
                seen.teleport = player.teleportSequence;
                cues.push_back({SoundEffect::Teleport, player.pos});
            }
            if (common::sequenceNewer(player.attackSequence, seen.attack)) {
                seen.attack = player.attackSequence;
                if (player.lastAttack != common::AttackKind::None)
                    cues.push_back({client::attackPresentation(player.lastAttack).castSound, player.pos});
            }
            if (common::sequenceNewer(player.spellSequence, seen.spell)) {
                seen.spell = player.spellSequence;
                cues.push_back(
                    {client::attackPresentation(player.spellEffect).impactSound, player.spellPosition});
            }
            for (const auto& event : player.damageEvents) {
                if (!common::sequenceNewer(event.sequence, seen.damage))
                    continue;
                seen.damage = event.sequence;
                if (event.amount <= 0)
                    continue;
                // Lethal punches still make contact; death replaces only the hazard hurt cue.
                if (event.source < 0 && !player.alive && event.sequence == player.damageSequence)
                    continue;
                const auto effect = event.source < 0 ? SoundEffect::SmallDamage : SoundEffect::Punch;
                cues.push_back({effect, event.contact});
            }
            if (seen.alive && !player.alive)
                cues.push_back({SoundEffect::Death, player.pos});
            if (!seen.alive && player.alive)
                cues.push_back({SoundEffect::Respawn, player.pos});
            seen.alive = player.alive;
        }
        return cues;
    }

  private:
    struct Seen {
        bool initialized = false, alive = false;
        std::uint32_t attack = 0, damage = 0, spell = 0, teleport = 0;
    };
    std::array<Seen, common::MAX_PLAYERS> seen_{};
};
