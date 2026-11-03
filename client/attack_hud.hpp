#include "ui_font.hpp"
#pragma once
#include "attack_presentation.hpp"
#include "common/common.hpp"
#include "common/keybindings.hpp"
#include "common/player_class.hpp"
#include "common/settings.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdio>

inline float attackSecondsRemaining(const common::PlayerState& player,
                                    common::AttackKind ability = common::AttackKind::None,
                                    const common::ServerSettings& settings = common::ServerSettings{}) {
    if (ability == common::AttackKind::None)
        ability = player.combatDebug.attack;
    float remaining = std::max(player.stunTicks, player.dodgeTicks) * common::TICK_DT;
    if (ability == common::AttackKind::Explosion)
        remaining = std::max(remaining, player.explosionCooldown * common::TICK_DT);
    if (ability == common::AttackKind::Beam)
        remaining = std::max(remaining, player.beamCooldown * common::TICK_DT);
    if (ability == common::AttackKind::Lightning)
        remaining = std::max(remaining, player.lightningCooldown * common::TICK_DT);
    if (player.combatDebug.attack == common::AttackKind::None)
        return remaining;
    const auto kind = player.combatDebug.attack;
    const auto& desc = common::attackDescription(kind, settings);
    const bool spell = common::isSpell(kind);
    const auto total =
        desc.startupTicks + desc.activeTicks + (!spell || ability == kind ? desc.recoveryTicks : 0);
    return std::max(remaining,
                    float(total - std::min(total, player.combatDebug.elapsedTicks)) * common::TICK_DT);
}
inline sf::String abilityBindingLabel(const std::vector<int>& bindings) {
    if (bindings.empty())
        return "Disabled";
    sf::String result;
    for (int code : bindings) {
        if (!result.isEmpty())
            result += "/";
        if (code >= int(sf::Keyboard::KeyCount)) {
            constexpr std::array<const char*, 5> mouseNames{"LMB", "RMB", "MMB", "Mouse 4", "Mouse 5"};
            result += mouseNames.at(code - int(sf::Keyboard::KeyCount));
        } else
            result +=
                sf::Keyboard::getDescription(sf::Keyboard::delocalize(static_cast<sf::Keyboard::Key>(code)));
    }
    return result;
}
inline void drawAttackHud(sf::RenderTarget& target, const sf::Font& font, const common::PlayerState& player,
                          const common::KeyBindings& bindings = {},
                          const common::ServerSettings& settings = common::ServerSettings{}) {
    if (!player.connected)
        return;
    const float width = std::min(172.f, (target.getView().getSize().x - 40) / 5 - 8);
    const float left = (target.getView().getSize().x - (5 * width + 32)) / 2,
                y = target.getView().getSize().y - 74;
    const auto& selectedClass = common::classDefinition(player.playerClass);
    sf::Text classLabel(font, std::string("Class: ") + selectedClass.name + " | " + selectedClass.perk,
                        uiFontSize(14));
    classLabel.setPosition({left, y - 24});
    classLabel.setFillColor(sf::Color(255, 215, 140));
    classLabel.setOutlineColor(sf::Color(20, 23, 28));
    classLabel.setOutlineThickness(1.f);
    target.draw(classLabel);
    char dodgeStatus[32];
    if (player.dodgeTicks)
        std::snprintf(dodgeStatus, sizeof(dodgeStatus), "DODGING");
    else if (player.dodgeCooldown)
        std::snprintf(dodgeStatus, sizeof(dodgeStatus), "%.1fs", player.dodgeCooldown * common::TICK_DT);
    else
        std::snprintf(dodgeStatus, sizeof(dodgeStatus), "Ready");
    sf::Text dodgeLabel(font,
                        sf::String("DODGE (") + abilityBindingLabel(bindings[common::Action::Dodge]) + ") " +
                            dodgeStatus,
                        uiFontSize(14));
    const auto dodgeBounds = dodgeLabel.getLocalBounds();
    dodgeLabel.setPosition({target.getView().getSize().x - left - dodgeBounds.size.x, y - 24});
    dodgeLabel.setFillColor(player.dodgeTicks ? sf::Color(100, 240, 255) : sf::Color(180, 215, 240));
    dodgeLabel.setOutlineColor(sf::Color(20, 23, 28));
    dodgeLabel.setOutlineThickness(1.f);
    target.draw(dodgeLabel);
    for (unsigned i = 0; i < 5; ++i) {
        const auto ability = static_cast<common::AttackKind>(i + 1);
        const float remaining = attackSecondsRemaining(player, ability, settings);
        const float x = left + i * (width + 8);
        const bool selected = static_cast<unsigned>(player.combatDebug.attack) == i + 1;
        const auto& presentation = client::attackPresentation(ability);
        const sf::String title = sf::String(presentation.name) + " (" +
                                 abilityBindingLabel(bindings[presentation.bindingAction]) + ")";
        sf::Text name(font, title, uiFontSize(13));
        if (name.getLocalBounds().size.x > width - 16)
            name.setScale({(width - 16) / name.getLocalBounds().size.x, 1});
        name.setPosition({x + 8, y + 6});
        name.setFillColor(selected ? sf::Color(255, 200, 70) : sf::Color::White);
        target.draw(name);
        float ready = 1;
        if (remaining > 0) {
            float duration = std::max(common::TICK_DT, static_cast<float>(settings.damageStunSeconds));
            if (common::isSpell(ability))
                duration = std::max(duration, common::attackDescription(ability, settings).recoveryTicks *
                                                  common::TICK_DT);
            if (player.combatDebug.attack != common::AttackKind::None) {
                const auto& desc = common::attackDescription(player.combatDebug.attack, settings);
                duration = std::max(duration, (desc.startupTicks + desc.activeTicks + desc.recoveryTicks) *
                                                  common::TICK_DT);
            }
            ready = 1 - remaining / duration;
        }
        sf::RectangleShape progress({width * std::clamp(ready, 0.f, 1.f), 3});
        progress.setPosition({x, y + 28});
        progress.setFillColor(sf::Color(100, 175, 255));
        target.draw(progress);
    }
}
