#include "common/team_spawns.hpp"
#include "common/bot_ai.hpp"
#include <fstream>
#include <filesystem>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    common::ServerSettings rulesUnderTest;
    check(argc==2,"Arena path required");
    auto temp=std::filesystem::temp_directory_path()/"marpg-ffa-settings.toml";
    std::ofstream(temp)<<"[server]\nteams=0\n";
    auto settings=common::loadServerSettings(temp.string());
    check(settings.teams==0,"Cannot configure free for all");
    sf::Packet packet;common::writeSettings(packet,settings);common::ServerSettings received;
    check(common::readSettings(packet,received) && received.teams==0,"FFA settings not transmitted");
    std::ofstream(temp)<<"[server]\nslots=32\nteams=32\n";
    auto maxTeams=common::loadServerSettings(temp.string());
    check(maxTeams.teams==32,"Team count cannot reach slots");
    sf::Packet maxPacket;common::writeSettings(maxPacket,maxTeams);
    check(common::readSettings(maxPacket,received) && received.teams==32,"32 teams did not survive network settings");
    std::ofstream(temp)<<"[client]\nteam=32\n";
    check(common::loadClientSettings(temp.string()).team==32,"Cannot request team 32");
    std::ofstream(temp)<<"[server]\nslots=32\nteams=33\n";
    bool rejected=false;try{common::loadServerSettings(temp.string());}catch(const std::exception& e){rejected=std::string(e.what()).find("0-32")!=std::string::npos;}
    std::filesystem::remove(temp);check(rejected,"Teams error does not explain valid range");
    rulesUnderTest = settings;
    common::CollisionWorld walls;walls.load(argv[1]);common::TriggerSystem hazards;hazards.load(argv[1],rulesUnderTest);
    common::TeamSpawns spawns;spawns.load(argv[1],0,walls,hazards);
    check(spawns.groups().size()==1,"FFA spawns were split into teams");
    auto spawn=spawns.choose(static_cast<unsigned>(-1),{},walls,hazards);check(spawn.has_value(),"Teamless player cannot spawn");
    check(common::chooseTeam({},settings,2)==0,"FFA should ignore requested teams");
    for(auto kind:{common::AttackKind::Light,common::AttackKind::Heavy,common::AttackKind::Explosion,common::AttackKind::Lightning}) {
        common::PlayerState a,b;a.connected=b.connected=true;a.team=b.team=-1;a.pos={0,0};b.pos={30,0};
        common::CombatState combat;common::CollisionWorld empty;
        auto aim=(kind==common::AttackKind::Light || kind==common::AttackKind::Heavy)?sf::Vector2f{1,0}:b.pos;
        check(common::startAttack(a,combat,kind,aim,rulesUnderTest),"Cannot attack in FFA");
        for(int tick=0;tick<500 && b.health==100;++tick)common::updateAttack(a,combat,{&a,&b},empty,{},rulesUnderTest);
        check(b.health<100,"FFA damage blocked by friendly-fire setting");
    }
    common::Navigation nav(argv[1],walls,hazards);common::BotAI ai(nav,walls,42,rulesUnderTest);
    common::PlayerState bot,enemy;bot.connected=enemy.connected=true;bot.team=enemy.team=-1;
    bot.pos=*spawn;enemy.pos=*spawn+sf::Vector2f{30,0};common::CombatState combat;
    ai.update(0,bot,combat,{&bot,&enemy});check(ai.target(0)==1,"FFA bot ignored teamless opponent");
}
