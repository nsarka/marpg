#pragma once
#include "settings.hpp"
#include "collision_world.hpp"
#include "trigger_system.hpp"
#include <algorithm>
#include <numeric>

namespace common {
class TeamSpawns {
    bool freeForAll_=false;
public:
    void load(const std::string& path,unsigned teams,const CollisionWorld& walls,const TriggerSystem& triggers) {
        tmx::Map map;if(!map.load(path))throw std::runtime_error("Cannot load spawn map");
        std::vector<sf::Vector2f> points;
        for(const auto& layer:map.getLayers()) {
            if(layer->getType()!=tmx::Layer::Type::Object || layer->getName()!="Spawns")continue;
            for(const auto& object:layer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
                auto p=object.getPosition();auto offset=layer->getOffset();
                sf::Vector2f world{(p.x-p.y)*map.getTileSize().x/(2.f*map.getTileSize().y)+offset.x,(p.x+p.y)*.5f+offset.y};
                if(walls.overlaps(world) || triggers.contains(world))throw std::runtime_error("Unsafe spawn: "+object.getName());
                points.push_back(world);
            }
        }
        freeForAll_=teams==0;
        teams=std::max(1u,teams);
        if(teams>MAX_PLAYERS)throw std::runtime_error("Spawn team count exceeds maximum player slots");
        if(points.size()<teams)throw std::runtime_error("Spawns layer requires at least one safe spawn point per team: found "+std::to_string(points.size())+" for "+std::to_string(teams)+" teams");
        groups_.assign(teams,{}); cursors_.assign(teams,0);
        partition(std::move(points),0,teams);
    }
    const std::vector<std::vector<sf::Vector2f>>& groups() const{return groups_;}
    std::optional<sf::Vector2f> choose(unsigned team,const std::vector<PlayerState>& players,
                                     const CollisionWorld& walls,const TriggerSystem& triggers) {
        if(freeForAll_)team=0;
        if(team>=groups_.size())return std::nullopt;
        const auto& group=groups_[team];
        // Rotate through the team's points; nearby offsets accommodate occupied points.
        for(int ring=0;ring<=6;++ring)for(std::size_t i=0;i<group.size();++i) {
            const auto index=(cursors_[team]+i)%group.size();
            for(int step=0;step<(ring?16:1);++step) {
                float angle=step*6.2831853f/16.f;
                auto p=group[index]+sf::Vector2f{std::cos(angle),std::sin(angle)}*float(ring*24);
                if(walls.overlaps(p) || triggers.contains(p))continue;
                bool occupied=false;
                for(const auto& other:players)if(other.connected && other.alive && (other.pos-p).length()<21.f){occupied=true;break;}
                if(!occupied){cursors_[team]=(index+1)%group.size();return p;}
            }
        }
        return std::nullopt;
    }
private:
    void partition(std::vector<sf::Vector2f> points,unsigned first,unsigned teams) {
        if(teams==1){groups_[first]=std::move(points);return;}
        auto low=points.front(),high=low;
        for(auto p:points){low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);}
        bool x=high.x-low.x>=high.y-low.y;
        std::sort(points.begin(),points.end(),[x](auto a,auto b){return x ? (a.x==b.x?a.y<b.y:a.x<b.x):(a.y==b.y?a.x<b.x:a.y<b.y);});
        unsigned left=teams/2;auto split=points.size()*left/teams;
        partition({points.begin(),points.begin()+split},first,left);
        partition({points.begin()+split,points.end()},first+left,teams-left);
    }
    std::vector<std::vector<sf::Vector2f>> groups_;
    std::vector<std::size_t> cursors_;
};
inline unsigned smallestTeam(const std::vector<PlayerState>& players,unsigned teams) {
    if(!teams)return 0;
    std::vector<unsigned> count(teams);
    for(const auto& p:players)if(p.connected && p.team>=0 && unsigned(p.team)<teams)++count[p.team];
    return std::min_element(count.begin(),count.end())-count.begin();
}
// Requests use 1-based team numbers; zero means automatic assignment.
inline unsigned chooseTeam(const std::vector<PlayerState>& players,const ServerSettings& settings,unsigned request) {
    const auto automatic=smallestTeam(players,settings.teams);
    if(request==0 || request>settings.teams)return automatic;
    const auto preferred=request-1;
    if(settings.honorTeamRequests)return preferred;
    unsigned smallestCount=0,preferredCount=0;
    for(const auto& player:players)if(player.connected) {
        if(player.team==static_cast<int>(automatic))++smallestCount;
        if(player.team==static_cast<int>(preferred))++preferredCount;
    }
    return preferredCount==smallestCount?preferred:automatic;
}

}
