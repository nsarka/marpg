#pragma once
#include "navigation.hpp"
#include "combat_system.hpp"
#include <random>
#include <deque>

namespace common {
class BotAI {
public:
    BotAI(const Navigation& navigation,const CollisionWorld& walls,unsigned seed=std::random_device{}())
        : navigation_(navigation),walls_(walls),random_(seed) {}
    void reset(PlayerId id) {brains_[id]=Brain{};}
    int target(PlayerId id) const{return brains_[id].target;}
    void update(PlayerId id,PlayerState& bot,CombatState& combat,const std::vector<PlayerState*>& players) {
        auto& brain=brains_[id];
        if(!bot.connected || !bot.alive){reset(id);return;}
        auto enemy=[&](int i){return i>=0 && std::size_t(i)<players.size() && players[i]!=&bot && players[i]->connected &&
            players[i]->alive && players[i]->health>0 && (activeSettings.teams==0 || (players[i]->team>=0 && players[i]->team!=bot.team));};
        for(const auto& hit:bot.damageEvents) {
            if(!sequenceNewer(hit.sequence,seenDamage_[id]))continue;
            seenDamage_[id]=hit.sequence;
            if(hit.amount<=0 || !enemy(hit.source))continue;
            bool claimed=false;
            for(std::size_t other=0;other<players.size() && other<activeSettings.bots;++other)
                if(other!=id && players[other]->connected && players[other]->alive && brains_[other].target==hit.source){claimed=true;break;}
            if(!claimed){brain=Brain{};brain.target=hit.source;}
        }
        if(!enemy(brain.target) || ++brain.targetAge>=12*TICK_RATE) {
            std::vector<int> enemies;
            for(std::size_t i=0;i<players.size();++i)if(enemy(i))enemies.push_back(i);
            brain=Brain{};
            if(enemies.empty()){bot.vel={};return;}
            brain.target=enemies[std::uniform_int_distribution<std::size_t>(0,enemies.size()-1)(random_)];
        }
        auto& victim=*players[brain.target];
        auto delta=victim.pos-bot.pos;float distance=delta.length();
        std::vector<sf::Vector2f> blockers,traffic;
        for(auto* p:players)if(p!=&bot && p->connected && p->alive) {
            blockers.push_back(p->pos);if(p!=&victim)traffic.push_back(p->pos);
        }
        updateAim(bot,combat,victim.pos,walls_);
        if(brain.pause) --brain.pause;
        const bool clear=attackPathClear(bot.pos,victim.pos,walls_);
        if(brain.combo.empty() && combat.attack==AttackKind::None && !brain.pause) {
            const int count=std::uniform_int_distribution<int>(2,4)(random_);
            constexpr std::array<AttackKind,4> choices{AttackKind::Jab,AttackKind::Hook,AttackKind::Uppercut,AttackKind::Lightning};
            for(int i=0;i<count;++i)brain.combo.push_back(choices[std::uniform_int_distribution<int>(0,3)(random_)]);
        }
        if(!brain.combo.empty() && combat.attack==AttackKind::None && !brain.pause && clear) {
            const auto kind=brain.combo.front();
            const bool spell=kind==AttackKind::Uppercut || kind==AttackKind::Lightning;
            const float range=kind==AttackKind::Uppercut?activeSettings.spell.range:
                kind==AttackKind::Lightning?activeSettings.lightning.range:std::min(58.f,attackDescription(kind).range);
            if(distance<=range && startAttack(bot,combat,kind,spell?victim.pos:delta)) {
                brain.combo.pop_front();
                if(brain.combo.empty())brain.pause=TICK_RATE+std::uniform_int_distribution<int>(8,24)(random_);
            }
        }
        // Stay at the casting position through windup/channeling and recovery.
        if(combat.attack==AttackKind::Uppercut || combat.attack==AttackKind::Lightning || (clear && distance<=50)) {
            bot.vel={};brain.stuck=0;return;
        }
        if(brain.repath)--brain.repath;
        if(!brain.repath) {
            brain.path=navigation_.path(bot.pos,victim.pos,traffic);brain.waypoint=0;
            brain.repath=TICK_RATE/2+id%9;
            if(brain.path.empty()){
                bot.vel={};if(++brain.failures>=3){brain.target=-1;brain.failures=0;}return;
            }
            brain.failures=0;
        }
        if(brain.path.empty()){bot.vel={};return;}
        while(brain.waypoint<brain.path.size() && (brain.path[brain.waypoint]-bot.pos).length()<8)++brain.waypoint;
        // Short lookahead smooths grid corners without cutting through walls or hazards.
        const auto lookahead=std::min(brain.path.size(),brain.waypoint+5);
        for(std::size_t next=brain.waypoint+1;next<lookahead;++next) {
            bool occupied=false;
            auto segment=brain.path[next]-bot.pos;float length=segment.lengthSquared();
            for(auto p:traffic){float t=length>0?std::clamp((p-bot.pos).dot(segment)/length,0.f,1.f):0;
                if((bot.pos+segment*t-p).length()<28){occupied=true;break;}}
            if(occupied || !navigation_.clear(bot.pos,brain.path[next]))break;
            brain.waypoint=next;
        }
        if(brain.waypoint>=brain.path.size()){bot.vel={};brain.repath=0;return;}
        auto toward=brain.path[brain.waypoint]-bot.pos;
        float length=toward.length();if(length<.001f){bot.vel={};return;}
        auto step=toward/length*std::min(length,RUN_SPEED*TICK_DT);
        auto next=walls_.move(bot.pos,step,CollisionWorld::PlayerRadius,blockers);
        if((next-bot.pos).length()<step.length()*.25f) {
            ++brain.stuck;
            // Sidestep crowds, using the same collision and floor safety as normal movement.
            sf::Vector2f side{-step.y,step.x};if(id%2)side=-side;
            for(auto alternative:{step*.3f+side,step*.3f-side}) {
                if(alternative.length()>step.length())alternative=alternative.normalized()*step.length();
                auto candidate=walls_.move(bot.pos,alternative,CollisionWorld::PlayerRadius,blockers);
                if((candidate-bot.pos).length()>(next-bot.pos).length() && navigation_.clear(bot.pos,candidate))next=candidate;
            }
            if(brain.stuck>=TICK_RATE/4){brain.repath=0;brain.stuck=0;}
        }else brain.stuck=0;
        if(!navigation_.clear(bot.pos,next))next=bot.pos;
        bot.vel=(next-bot.pos)/TICK_DT;bot.pos=next;
    }
private:
    struct Brain {int target=-1;Tick targetAge=0,repath=0,pause=0,stuck=0;unsigned failures=0;
        std::vector<sf::Vector2f> path;std::size_t waypoint=0;std::deque<AttackKind> combo;};
    const Navigation& navigation_;const CollisionWorld& walls_;std::mt19937 random_;
    std::array<Brain,MAX_PLAYERS> brains_;
    std::array<std::uint32_t,MAX_PLAYERS> seenDamage_{};
};
}
