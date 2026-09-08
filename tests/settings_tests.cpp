#include "common/settings.hpp"
#include "common/team_spawns.hpp"
#include "common/combat_system.hpp"
#include <filesystem>
#include <fstream>
#include <set>
#include <iostream>
#include <chrono>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==2,"Project path required");std::filesystem::path root=argv[1];
    auto temp=std::filesystem::temp_directory_path()/("marpg-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".toml");
    for(const auto& text:{"[server]\nplayers = -1\n","[damage]\ntrigger_bpm = 0\n","[server]\nport = 70000\n","[server]\nteams = \"two\"\n","[server]\nplyers = 12\n","[server\n"}) {
        {std::ofstream file(temp);file<<text;}
        bool rejected=false;try{common::loadServerSettings(temp.string());}catch(const std::exception&){rejected=true;}
        std::filesystem::remove(temp);check(rejected,"Invalid TOML setting was silently accepted");
    }
    auto settings=common::loadServerSettings((root/"server.toml").string());
    auto client=common::loadClientSettings((root/"client.toml").string());
    check(settings.teams==2 && settings.players==20 && client.port==54000,"Default files not loaded");
    settings.port=55001;settings.players=12;settings.teams=3;settings.triggerDamage=7;settings.triggerBpm=60;
    settings.boundsDamage=9;settings.boundsBpm=240;settings.jabDamage=27;settings.hookDamage=40;
    sf::Packet packet;common::writeSettings(packet,settings);common::ServerSettings received;
    check(common::readSettings(packet,received) && received.port==55001 && received.teams==3 && received.boundsBpm==240,"Settings wire roundtrip failed");
    common::applySettings(settings);
    check(common::attackDescription(common::AttackKind::Jab).damage==27 && common::attackDescription(common::AttackKind::Hook).damage==40,"Combat ignores config");
    common::TriggerSystem triggers;triggers.load((root/"assets/tiled/Demo.tmx").string());
    common::PlayerState player;player.connected=true;player.pos={-512,1024};
    triggers.update(0,player);check(player.health==93,"Configured trigger entry damage");
    for(int i=0;i<63;++i)triggers.update(0,player);check(player.health==93,"Trigger beat too early");
    triggers.update(0,player);check(player.health==86,"Configured trigger BPM");
    player.pos={128,-1};triggers.update(0,player);check(player.health==77,"Configured bounds entry damage");
    for(int i=0;i<16;++i)triggers.update(0,player);check(player.health==68,"Configured bounds BPM");
    common::CollisionWorld walls;walls.load((root/"assets/tiled/Demo.tmx").string());
    for(unsigned teams=1;teams<=20;++teams){
        common::TeamSpawns spawns;spawns.load((root/"assets/tiled/Demo.tmx").string(),teams,walls,triggers);
        unsigned count=0;std::set<std::uint32_t> colors;
        std::vector<common::PlayerState> occupants;
        for(unsigned team=0;team<teams;++team){
            check(!spawns.groups()[team].empty(),"Team without spawns");count+=spawns.groups()[team].size();
            colors.insert(common::teamColor(team,teams).toInteger());
            auto p=spawns.choose(team,occupants,walls,triggers);check(p.has_value(),"No safe team spawn");
            common::PlayerState occupant;occupant.connected=true;occupant.team=team;occupant.pos=*p;occupants.push_back(occupant);
        }
        check(count==20 && colors.size()==teams,"Spawns/colors not unique or complete");
    }
    check(common::teamColor(0,2)==sf::Color(255,70,70,220),"Red changed");
    check(common::teamColor(1,2)==sf::Color(70,70,255,220),"Blue wrong");
    common::PlayerState attacker,target;attacker.connected=target.connected=true;attacker.team=target.team=0;
    attacker.pos={0,0};target.pos={30,0};common::CombatState combat;common::CollisionWorld empty;
    common::startAttack(attacker,combat,common::AttackKind::Jab,{1,0});
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,{&attacker,&target},empty);
    check(target.health==100,"Friendly fire should be disabled");
    auto friendly=settings;friendly.friendlyFire=true;common::applySettings(friendly);
    common::startAttack(attacker,combat,common::AttackKind::Jab,{1,0});
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,{&attacker,&target},empty);
    check(target.health==73,"Configured friendly fire did not enable teammate damage");
    target.health=100;common::applySettings(settings);
    target.team=1;common::startAttack(attacker,combat,common::AttackKind::Hook,{1,0});
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,{&attacker,&target},empty);
    check(target.health==60,"Enemy/configured hook damage");
    common::respawn(target,combat,{10,20});check(target.team==1,"Respawn changed team");
    auto invalid=settings;invalid.teams=0;bool failed=false;try{invalid.validate();}catch(...){failed=true;}check(failed,"Invalid config accepted");
    common::applySettings(common::ServerSettings{});
    std::cout<<"PASS: TOML, settings sync, damage/BPM, teams, friendly fire, 20 safe spawns for every team count\n";
}
