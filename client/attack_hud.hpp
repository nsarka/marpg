#pragma once
#include "common/common.hpp"
#include "common/keybindings.hpp"
#include "common/settings.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdio>

inline float attackSecondsRemaining(const common::PlayerState& player,common::AttackKind ability=common::AttackKind::None) {
    if(ability==common::AttackKind::None)ability=player.combatDebug.attack;
    float remaining=player.stunTicks*common::TICK_DT;
    if(ability==common::AttackKind::Uppercut)remaining=std::max(remaining,player.explosionCooldown*common::TICK_DT);
    if(ability==common::AttackKind::Lightning)remaining=std::max(remaining,player.lightningCooldown*common::TICK_DT);
    if(player.combatDebug.attack==common::AttackKind::None)return remaining;
    const auto kind=player.combatDebug.attack;
    const auto& desc=common::attackDescription(kind);
    const bool spell=kind==common::AttackKind::Uppercut || kind==common::AttackKind::Lightning;
    const auto total=desc.startupTicks+desc.activeTicks+(!spell || ability==kind?desc.recoveryTicks:0);
    return std::max(remaining,float(total-std::min(total,player.combatDebug.age))*common::TICK_DT);
}
inline sf::String abilityBindingLabel(const std::vector<int>& bindings) {
    if(bindings.empty())return "Disabled";
    sf::String result;
    for(int code:bindings) {
        if(!result.isEmpty())result+="/";
        if(code>=int(sf::Keyboard::KeyCount)) {
            constexpr std::array<const char*,5> mouseNames{"LMB","RMB","MMB","Mouse 4","Mouse 5"};
            result+=mouseNames.at(code-int(sf::Keyboard::KeyCount));
        }else result+=sf::Keyboard::getDescription(sf::Keyboard::delocalize(static_cast<sf::Keyboard::Key>(code)));
    }
    return result;
}
inline void drawAttackHud(sf::RenderTarget& target,const sf::Font& font,const common::PlayerState& player,const common::KeyBindings& bindings = {}) {
    if(!player.connected)return;
    constexpr std::array<const char*,4> names{"JAB","HOOK","EXPLOSION","LIGHTNING"};
    const float width=std::min(172.f,(target.getView().getSize().x-40)/4-8);
    const float left=(target.getView().getSize().x-(4*width+24))/2,y=target.getView().getSize().y-74;
    for(unsigned i=0;i<4;++i) {
        const auto ability=static_cast<common::AttackKind>(i+1);
        const float remaining=attackSecondsRemaining(player,ability);
        const float x=left+i*(width+8);
        const bool selected=static_cast<unsigned>(player.combatDebug.attack)==i+1;
        constexpr std::array<unsigned,4> actions{5,6,9,10};
        const sf::String title=sf::String(names[i])+" ("+abilityBindingLabel(bindings.actions[actions[i]])+")";
        sf::Text name(font,title,13);
        if(name.getLocalBounds().size.x>width-16)name.setScale({(width-16)/name.getLocalBounds().size.x,1});name.setPosition({x+8,y+6});name.setFillColor(selected?sf::Color(255,200,70):sf::Color::White);target.draw(name);
        float ready=1;
        if(remaining>0) {
            float duration=std::max(common::TICK_DT,static_cast<float>(common::activeSettings.damageStunSeconds));
            if(ability==common::AttackKind::Uppercut || ability==common::AttackKind::Lightning)
                duration=std::max(duration,common::attackDescription(ability).recoveryTicks*common::TICK_DT);
            if(player.combatDebug.attack!=common::AttackKind::None) {
                const auto& desc=common::attackDescription(player.combatDebug.attack);
                duration=std::max(duration,(desc.startupTicks+desc.activeTicks+desc.recoveryTicks)*common::TICK_DT);
            }
            ready=1-remaining/duration;
        }
        sf::RectangleShape progress({width*std::clamp(ready,0.f,1.f),3});progress.setPosition({x,y+28});progress.setFillColor(sf::Color(100,175,255));target.draw(progress);
    }
}
