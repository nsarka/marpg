#include "ui_font.hpp"
#pragma once
#include "common/combat_system.hpp"
#include "player.hpp"
#include <SFML/Graphics.hpp>

inline void drawCombatDebug(sf::RenderTarget& target, std::vector<Player>& players,
                            const common::CollisionWorld& walls, const sf::Font& font,const common::ServerSettings& settings=common::ServerSettings{}) {
    for (auto& player : players) {
        const auto& state=player.state();
        const auto& debug=state.combatDebug;
        if (!state.connected || !state.alive || debug.attack==common::AttackKind::None) continue;
        const auto& attack=common::attackDescription(debug.attack, settings);
        const bool active=debug.age>=attack.startupTicks && debug.age<attack.startupTicks+attack.activeTicks;
        const char* phase=debug.hit ? "HIT" : debug.age<attack.startupTicks ? "WINDUP" : active ? "ACTIVE" : "RECOVERY";
        sf::Color color=debug.hit ? sf::Color(60,255,130) : debug.age<attack.startupTicks ? sf::Color(255,200,60)
                                    : active ? sf::Color(255,70,90) : sf::Color(150,160,180);
        if((debug.attack==common::AttackKind::Explosion || debug.attack==common::AttackKind::Lightning)) {
            const auto center=state.spellPosition;
            sf::CircleShape area(attack.range);area.setOrigin({attack.range,attack.range});area.setPosition(center);
            area.setFillColor(sf::Color(color.r,color.g,color.b,35));area.setOutlineColor(color);area.setOutlineThickness(1.f);target.draw(area);
            const bool castClear=common::attackPathClear(state.pos,center,walls);
            const auto castColor=castClear?color:sf::Color(255,70,90);
            sf::Vertex castLine[]={{state.pos,castColor},{center,castColor}};target.draw(castLine,2,sf::PrimitiveType::Lines);
            for(auto& candidate:players) {
                const auto& other=candidate.state();
                if(!other.connected || !other.alive || (other.pos-center).length()>attack.range)continue;
                if(&other!=&state && !settings.friendlyFire && state.team>=0 && state.team==other.team)continue;
                const bool clear=castClear && common::attackPathClear(center,other.pos,walls);
                const auto tint=clear?sf::Color(80,220,255):sf::Color(255,70,90);
                sf::Vertex line[]={{center,tint},{other.pos,tint}};target.draw(line,2,sf::PrimitiveType::Lines);
                sf::CircleShape mark(4);mark.setOrigin({4,4});mark.setPosition(other.pos);mark.setFillColor(tint);target.draw(mark);
            }
            const std::string spellPhase=debug.hit?"DETONATED":phase;
            sf::Text label(font,std::string(debug.attack==common::AttackKind::Lightning?"LIGHTNING ":"SPELL ")+spellPhase,uiFontSize(12));label.setFillColor(color);
            label.setOutlineColor(sf::Color::Black);label.setOutlineThickness(1.f);
            label.setPosition(center+sf::Vector2f{-45,16});target.draw(label);
            continue;
        }
        const float angle=std::atan2(debug.direction.y,debug.direction.x);
        constexpr unsigned segments=32;
        sf::ConvexShape sector(segments+2);
        sector.setPoint(0,state.pos);
        for (unsigned i=0;i<=segments;++i) {
            const float a=angle-common::attackHalfAngle(debug.attack, settings)+(2.f*common::attackHalfAngle(debug.attack, settings))*float(i)/segments;
            sector.setPoint(i+1,state.pos+sf::Vector2f{std::cos(a),std::sin(a)}*attack.range);
        }
        sector.setFillColor(sf::Color(color.r,color.g,color.b,35));
        sector.setOutlineColor(color);sector.setOutlineThickness(1.f);
        target.draw(sector);
        sf::Vertex heading[]={{state.pos,color},{state.pos+debug.direction*attack.range,color}};
        target.draw(heading,2,sf::PrimitiveType::Lines);
        for (std::size_t i=0;i<players.size();++i) {
            const auto& other=players[i].state();
            if (&other==&state || !other.connected) continue;
            const bool confirmed=debug.hit && debug.target==static_cast<std::int32_t>(i);
            const auto delta=other.pos-state.pos;
            if (!confirmed && (!other.alive || delta.length()>attack.range)) continue;
            if (!settings.friendlyFire && state.team>=0 && state.team==other.team) continue;
            const bool inArc=common::inAttackArc(delta,debug.direction,attack.range,debug.attack, settings);
            const bool clear=common::attackPathClear(state.pos,other.pos,walls);
            const auto lineColor=confirmed ? sf::Color(60,255,130) : !inArc ? sf::Color(130,140,160)
                                          : !clear ? sf::Color(255,70,90) : sf::Color(80,220,255);
            sf::Vertex line[]={{state.pos,lineColor},{other.pos,lineColor}};
            target.draw(line,2,sf::PrimitiveType::Lines);
            sf::CircleShape mark(confirmed ? 5.f : 3.f);
            mark.setOrigin({mark.getRadius(),mark.getRadius()});mark.setPosition(other.pos);
            mark.setFillColor(lineColor);target.draw(mark);
        }
        const std::string label=std::string(debug.attack==common::AttackKind::Light ? "LIGHT " : "HEAVY ")+phase;
        sf::Text text(font,label,uiFontSize(12));text.setFillColor(color);
        text.setOutlineColor(sf::Color::Black);text.setOutlineThickness(1.f);
        text.setPosition(state.pos+sf::Vector2f{-35,16});target.draw(text);
    }
}
