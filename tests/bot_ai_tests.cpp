#include "common/bot_ai.hpp"
#include <iostream>
#include <set>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==2,"Map path required");
    common::TriggerSystem hazards;hazards.load(argv[1]);
    common::CollisionWorld walls;walls.addPolygon({{50,500},{150,500},{150,800},{50,800}});
    common::Navigation nav(argv[1],walls,hazards);
    sf::Vector2f start{-100,650},goal{300,650};
    check(!nav.clear(start,goal),"Test wall must block direct travel");
    auto path=nav.path(start,goal);check(!path.empty(),"A* must find a route around the wall");
    auto previous=start;
    for(auto point:path){check(nav.clear(previous,point),"Path crossed a wall or hazard");previous=point;}
    check(nav.path(start,{128,-100}).empty(),"AI must not path off the floor");
    common::PlayerState bot,enemy,ally;
    bot.connected=enemy.connected=ally.connected=true;bot.team=ally.team=0;enemy.team=1;
    bot.pos=start;enemy.pos=goal;enemy.health=10000;bot.health=10000;ally.pos={-250,650};
    common::CombatState combat;common::BotAI ai(nav,walls,123);
    std::vector<common::PlayerState*> players{&bot,&enemy,&ally};
    std::set<common::AttackKind> attacks;
    for(int tick=0;tick<90*common::TICK_RATE;++tick){
        if(tick==128)enemy.pos={300,700}; // Replan for a moving target.
        auto old=bot.pos;
        ai.update(0,bot,combat,players);
        check((bot.pos-old).length()<=common::RUN_SPEED*common::TICK_DT+.01f,"AI exceeded player run speed");
        check(nav.safePoint(bot.pos),"AI entered a wall or hazard");
        check(ai.target(0)==1,"Bot targeted its teammate");
        if(combat.attack!=common::AttackKind::None)attacks.insert(combat.attack);
        if((combat.attack==common::AttackKind::Uppercut || combat.attack==common::AttackKind::Lightning) && combat.age==0) {
            check(combat.spellTarget==enemy.pos,"Bot spell did not target enemy world position");
            check(bot.vel.length()==0,"Bot should stand still while casting");
        }
        common::updateAttack(bot,combat,players,walls);
    }
    check(enemy.health<10000,"Bot never reached/attacked enemy behind wall");
    check(ally.health==100,"Bot damaged its teammate");
    check(attacks.size()==4,"Bot did not use all four attack types");
    enemy.connected=false;ally.team=1;
    ai.update(0,bot,combat,players);check(ai.target(0)==2,"Bot did not replace disconnected target");
    ally.alive=false;ai.update(0,bot,combat,players);check(ai.target(0)==-1 && bot.vel.length()==0,"Bot must idle without living enemies");
    enemy.connected=true;enemy.alive=true;ally.alive=true;std::set<int> chosen;
    for(int i=0;i<30;++i){ai.reset(0);ai.update(0,bot,combat,players);chosen.insert(ai.target(0));}
    check(chosen.size()==2,"Random target selection did not vary");
    bot.alive=false;ai.update(0,bot,combat,players);check(ai.target(0)==-1,"Death must clear bot target");
    std::cout<<"PASS: A* wall detours, safe floor, moving targets, mixed combos, teammate filtering, retargeting and death\n";
}
