#include "common/teleport_system.hpp"
#include "common/trigger_system.hpp"
#include "client/sound_events.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc==2,"Map path required");
    auto registry=std::make_shared<common::TriggerRegions>();
    common::TeleportSystem actual(registry);actual.load(argv[1]);
    common::TriggerSystem unified;unified.load(argv[1]);
    const auto loadedCount=unified.regions().all().size();
    check(loadedCount>=2,"Portal geometry missing from shared trigger registry");
    unified.addDamageTrigger({{0,0},{20,0},{20,20},{0,20}},2,30);
    check(unified.regions().all().size()==loadedCount+1,"Damage trigger not automatically registered");
    unified.load(argv[1]);
    check(unified.regions().all().size()==loadedCount,"Reload retained stale trigger regions");
    // Keep behavior tests independent of future edits to the user's level.
    const auto fixture=std::filesystem::temp_directory_path()/"marpg-teleport-test.tmx";
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::filesystem::remove(path);}} cleanup{fixture};
    std::ofstream(fixture)<<R"(<map version="1.10" orientation="isometric" width="20" height="20" tilewidth="128" tileheight="64">
    <objectgroup name="Triggers">
    <object id="1" name="portal_a" type="Teleport" x="793.109" y="642.04" width="14.3564" height="63.8614"><properties><property name="destination" value="portal_b"/></properties></object>
    <object id="2" name="portal_b" type="Teleport" x="597.069" y="131.644" width="17.8218" height="61.3861"><properties><property name="destination" value="portal_a"/></properties></object>
    </objectgroup></map>)";
    actual.load(fixture.string());
    check(registry->all().size()==2,"Portal reload retained stale debug geometry");
    {
        auto futureTrigger=registry->add({{0,0},{20,0},{20,20},{0,20}},sf::Color::Cyan);
        check(registry->all().size()==3 && registry->all().back()==futureTrigger,"New trigger kind requires debug registration");
        check(futureTrigger->contains({10,10}),"Debug region differs from gameplay geometry");
    }
    check(registry->all().size()==2,"Deleted trigger left a debug outline");
    // Rectangle coordinates are isometric object coordinates, not screen pixels.
    common::PlayerState p;p.connected=true;p.alive=true;
    auto project=[](float x,float y){return sf::Vector2f{x-y+64,(x+y)*.5f};};
    auto a=project(793.109f+14.3564f/2,642.04f+63.8614f/2);
    auto b=project(597.069f+17.8218f/2,131.644f+61.3861f/2);
    common::CollisionWorld walls;
    p.pos=a;p.vel={10,20};
    check(actual.update(0,p,walls),"Portal A did not activate");
    check((p.pos-b).length()<.01f && p.vel==sf::Vector2f{} && p.teleportSequence==1,"Wrong portal destination");
    for(int i=0;i<100;++i)check(!actual.update(0,p,walls),"Arrival teleported back immediately");
    p.pos={-10000,-10000};actual.update(0,p,walls);p.pos=b;
    check(actual.update(0,p,walls) && (p.pos-a).length()<.01f,"Return portal failed after leaving");
    common::PlayerState other;other.connected=true;other.pos=a;
    check(actual.update(1,other,walls),"Portal lock leaked between players");
    actual.reset(0);p.alive=false;p.pos=a;
    check(!actual.update(0,p,walls),"Dead player teleported");
    p.alive=true;
    walls.addPolygon({b+sf::Vector2f{-20,-20},b+sf::Vector2f{20,-20},b+sf::Vector2f{20,20},b+sf::Vector2f{-20,20}});
    check(!actual.update(0,p,walls),"Teleported inside a wall");
    SoundEvents events;std::vector<common::PlayerState> players(common::MAX_PLAYERS);players[0]=p;
    check(events.observe(players).empty(),"Join replayed teleport");
    ++players[0].teleportSequence;
    auto cues=events.observe(players);check(cues.size()==1 && cues[0].effect==SoundEffect::Teleport,"Teleport sound missing");
    check(events.observe(players).empty(),"Teleport sound repeated");
    sf::Packet packet;common::writeWorldPacket(packet,players);std::string type;packet>>type;
    std::vector<common::PlayerState> decoded;check(common::readWorldPacket(packet,decoded),"Teleport snapshot failed");
    check(decoded[0].teleportSequence==players[0].teleportSequence,"Teleport sequence not transmitted");
    std::ifstream fixtureInput(fixture);
    std::string damageMap((std::istreambuf_iterator<char>(fixtureInput)),{});fixtureInput.close();
    damageMap.insert(damageMap.find("</objectgroup>"),R"(<object id="3" name="Fantasy hazard" type="DamageTrigger" x="190" y="190" width="20" height="20"/>)");
    std::string floor="<layer name=\"Floor\" width=\"20\" height=\"20\"><data encoding=\"csv\">";
    for(int i=0;i<400;++i)floor+=(i?",1":"1");
    floor+="</data></layer>";damageMap.insert(damageMap.find("</map>"),floor);
    std::ofstream(fixture)<<damageMap;
    common::ServerSettings rules;rules.triggerDamage=7;rules.triggerInterval=.5;
    common::TriggerSystem hazards;hazards.load(fixture.string(),rules);
    check(hazards.regions().all().size()==3,"Object damage region missing from shared debug registry");
    common::PlayerState victim;victim.connected=true;victim.pos=project(200,200);
    hazards.update(0,victim);
    check(victim.health==93,"Object-layer damage trigger did not hit on entry");
    for(int i=0;i<31;++i)hazards.update(0,victim);
    check(victim.health==93,"Object-layer damage interval too short");
    hazards.update(0,victim);check(victim.health==86,"Object-layer damage did not repeat");
    victim.pos=project(250,250);hazards.update(0,victim);
    check(victim.health==86,"Damage continued outside object trigger");

}
