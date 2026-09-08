#pragma once
#include "common/settings.hpp"
#include "common/attack_delivery.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <optional>

class KillFeed {
public:
    static constexpr float Lifetime=7.f;
    static constexpr std::size_t MaxRows=6;
    struct Entry {common::KillEvent event;float age=0;bool local=false;};
    void observe(const std::vector<common::KillEvent>& events,common::PlayerId local) {
        if(!initialized_) {
            initialized_=true;
            if(!events.empty())seen_=events.back().sequence;
            return; // Joining a match must not replay historical kills.
        }
        for(const auto& event:events) {
            if(seen_ && !common::sequenceNewer(event.sequence,*seen_))continue;
            seen_=event.sequence;
            if(entries_.size()==MaxRows)entries_.erase(entries_.begin());
            entries_.push_back({event,0,event.victim==local || event.killer==static_cast<std::int32_t>(local)});
        }
    }
    void update(float dt) {
        for(auto& entry:entries_)entry.age+=std::max(0.f,dt);
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[](const auto& entry){return entry.age>=Lifetime;}),entries_.end());
    }
    const std::vector<Entry>& entries()const{return entries_;}
    static const char* cause(common::KillCause cause) {
        switch(cause){case common::KillCause::Jab:return "JAB";case common::KillCause::Hook:return "HOOK";
            case common::KillCause::Floor:return "FLOOR";case common::KillCause::Bounds:return "VOID";default:return "HIT";}
    }
    static float opacity(const Entry& entry){return std::clamp(Lifetime-entry.age,0.f,1.f);}
    void draw(sf::RenderTarget& target,const sf::Font& font) const {
        const float right=target.getView().getSize().x-18;
        const float limit=std::min(600.f,right-18);
        for(std::size_t i=0;i<entries_.size();++i) {
            const auto& entry=entries_[i];const auto& event=entry.event;const float fade=opacity(entry);
            auto tint=[&](sf::Color color){color.a=static_cast<std::uint8_t>(color.a*fade);return color;};
            sf::Text weapon(font,cause(event.cause),18);weapon.setStyle(sf::Text::Bold);
            const float methodWidth=weapon.getLocalBounds().size.x;
            const float nameWidth=std::max(20.f,(limit-methodWidth-24)*.5f);
            auto makeName=[&](const std::string& name){
                sf::Text text(font,sf::String::fromUtf8(name.begin(),name.end()),16);text.setStyle(sf::Text::Bold);
                auto original=text.getString();
                if(text.getLocalBounds().size.x>nameWidth) {
                    while(original.getSize()>0) {
                        original.erase(original.getSize()-1,1);text.setString(original+sf::String(U'…'));
                        if(text.getLocalBounds().size.x<=nameWidth)break;
                    }
                }
                return text;
            };
            auto killer=makeName(event.killerName),victim=makeName(event.victimName);
            const auto killerColor=event.killer<0?sf::Color(190,196,205):common::teamColor(event.killerTeam,common::activeSettings.teams);
            killer.setFillColor(tint(killerColor));victim.setFillColor(tint(common::teamColor(event.victimTeam,common::activeSettings.teams)));
            float width=killer.getLocalBounds().size.x+victim.getLocalBounds().size.x+methodWidth+24;
            float x=right-width+12*std::max(0.f,1-entry.age/.15f),y=18+float(i)*32;
            auto place=[&](sf::Text& text,float left){auto bounds=text.getLocalBounds();text.setPosition({left-bounds.position.x,y+12-bounds.position.y-bounds.size.y*.5f});};
            place(killer,x);target.draw(killer);
            float methodX=x+killer.getLocalBounds().size.x+12;
            weapon.setFillColor(tint(sf::Color::White));place(weapon,methodX);target.draw(weapon);
            place(victim,methodX+methodWidth+12);target.draw(victim);
        }
    }
private:
    bool initialized_=false;
    std::optional<std::uint32_t> seen_;
    std::vector<Entry> entries_;
};
