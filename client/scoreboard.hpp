#pragma once
#include "common/settings.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>

inline void drawScoreboard(sf::RenderTarget& target,const sf::Font& font,
                           const std::vector<common::PlayerState>& players,common::PlayerId local) {
    std::vector<std::vector<std::size_t>> teams(common::activeSettings.teams);
    for(std::size_t i=0;i<players.size();++i)
        if(players[i].connected && players[i].team>=0 && std::size_t(players[i].team)<teams.size())
            teams[players[i].team].push_back(i);
    std::array<unsigned,2> rows{};
    for(std::size_t t=0;t<teams.size();++t) {
        rows[t%2]+=2+teams[t].size();
        std::stable_sort(teams[t].begin(),teams[t].end(),[&](auto a,auto b){
            if(players[a].kills!=players[b].kills)return players[a].kills>players[b].kills;
            return players[a].deaths<players[b].deaths;
        });
    }
    const auto oldView=target.getView();
    const float height=100.f+26.f*std::max(rows[0],rows[1]);
    const float scale=std::min(oldView.getSize().x/940.f,oldView.getSize().y/(height+40.f));
    sf::View view(sf::FloatRect({0,0},oldView.getSize()/scale));target.setView(view);
    const float left=(view.getSize().x-900.f)/2,top=(view.getSize().y-height)/2;
    auto box=[&](float x,float y,float w,float h,sf::Color color){
        sf::RectangleShape rect({w,h});rect.setPosition({x,y});rect.setFillColor(color);target.draw(rect);
    };
    auto text=[&](sf::String value,float x,float y,unsigned size,sf::Color color,float maxWidth=1000.f){
        sf::Text label(font,value,size);label.setFillColor(color);label.setStyle(sf::Text::Bold);
        while(label.getLocalBounds().size.x>maxWidth && value.getSize()>1) {
            value.erase(value.getSize()-1,1);label.setString(value+sf::String(U'…'));
        }
        label.setPosition({x,y});target.draw(label);
    };
    box(left,top,900,height,sf::Color(18,23,31,240));
    const auto& serverName=common::activeSettings.name;
    text(sf::String::fromUtf8(serverName.begin(),serverName.end()),
         left+24,top+18,24,sf::Color::White,852);
    std::array<float,2> y{top+68,top+68};
    for(std::size_t t=0;t<teams.size();++t) {
        const auto col=t%2;const float x=left+24+col*444;
        auto color=common::teamColor(t,teams.size());color.a=255;
        box(x,y[col],408,2,color);
        y[col]+=8;
        text("PLAYER",x,y[col],13,sf::Color(165,175,190));
        text("KILLS",x+240,y[col],13,sf::Color(165,175,190));
        text("DEATHS",x+295,y[col],13,sf::Color(165,175,190));
        text("PING",x+365,y[col],13,sf::Color(165,175,190));y[col]+=24;
        for(auto id:teams[t]) {
            const auto& p=players[id];
            if(id==local)box(x-6,y[col]-1,420,26,sf::Color(255,255,255,25));
            text(sf::String::fromUtf8(p.name.begin(),p.name.end())+(p.alive?"":" *DEAD*"),x,y[col],17,color,226);
            text(std::to_string(p.kills),x+252,y[col],17,sf::Color::White);
            text(std::to_string(p.deaths),x+313,y[col],17,sf::Color::White);
            text(p.pingMs<0?"—":std::to_string(p.pingMs)+" ms",x+360,y[col],13,sf::Color(185,195,205),48);y[col]+=26;
        }
        y[col]+=22;
    }
    target.setView(oldView);
}
