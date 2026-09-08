#include "common/trigger_system.hpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==2,"Map path required");
    common::TriggerSystem triggers;
    triggers.load(argv[1]);
    check(triggers.outlines().size()==1,"Expected floor switch trigger");
    common::PlayerState player;
    player.connected=true;
    player.pos={-512,1024}; // Demo switch tile (5,10), image footprint center (128,448).
    triggers.update(1,player);
    check(player.health==98,"Entry must deal two damage immediately");
    for(int i=0;i<31;++i) triggers.update(1,player);
    check(player.health==98,"Second hit must wait the full 32-tick beat");
    triggers.update(1,player);
    check(player.health==96,"120 BPM requires a hit every 32 ticks");
    for(int i=0;i<32;++i) triggers.update(1,player);
    check(player.health==94,"Damage must repeat every beat");
    player.pos={-140,620};
    for(int i=0;i<128;++i) triggers.update(1,player);
    check(player.health==94,"Damage outside trigger");
    player.pos={-512,1024};
    triggers.update(1,player);
    check(player.health==92,"Reentry must damage immediately");
    for(int i=0;i<31;++i) triggers.update(1,player);
    check(player.health==92,"Reentry must reset beat timer");
    auto other=player;
    triggers.update(2,other);
    check(other.health==90,"Second player gets their own immediate entry hit");
    triggers.update(2,other);
    check(other.health==90,"Other player's timer must be independent");
    player.health=1;
    triggers.update(1,player);
    check(player.health==0 && !player.alive,"Lethal damage must clamp health and mark dead");
    for(int i=0;i<128;++i) triggers.update(1,player);
    check(player.health==0,"Dead players must not take further damage");
    check(triggers.hasFloor({128,64}),"First floor diamond center must be safe");
    check(triggers.hasFloor({128,1472}),"Last floor diamond center must be safe");
    check(!triggers.hasFloor({128,-1}),"Above map must have no floor");
    check(!triggers.hasFloor({128,1537}),"Below map must have no floor");
    check(!triggers.hasFloor({1400,10}),"Bounding rectangle corners are outside isometric floor");
    common::PlayerState falling;
    falling.connected=true; falling.pos={1400,10};
    triggers.update(3,falling);
    check(falling.health==98,"Missing floor must damage on entry");
    check(falling.damageEvents.back().source==-1 && falling.damageEvents.back().amount==2,
          "Missing floor must emit normal environmental damage feedback");
    for(int i=0;i<31;++i) triggers.update(3,falling);
    check(falling.health==98,"Missing floor must wait a full beat");
    triggers.update(3,falling);
    check(falling.health==96,"Missing floor must damage at 120 BPM");
    falling.pos={128,64};
    for(int i=0;i<64;++i) triggers.update(3,falling);
    check(falling.health==96,"Returning to floor must stop damage");
    falling.pos={128,-1}; triggers.update(3,falling);
    check(falling.health==94,"Missing floor reentry must damage immediately");
    auto second=falling; triggers.update(4,second);
    check(second.health==92,"Missing floor timers must be per player");
    triggers.reset(3); triggers.update(3,falling);
    check(falling.health==92,"Reset must clear missing floor timer");
    falling.health=1;
    for(int i=0;i<32;++i) triggers.update(3,falling);
    check(falling.health==0 && !falling.alive,"Missing floor must kill through normal damage path");
    triggers.update(3,falling);
    check(falling.health==0,"Missing floor must not damage dead players");
    common::TriggerSystem sample;
    sample.load((std::filesystem::path(argv[1]).parent_path()/"Sample.tmx").string());
    check(!sample.hasFloor({1664,832}),"Empty floor tile inside map dimensions must be hazardous");
    common::CollisionWorld collision;
    collision.load(argv[1]);
    check(!collision.overlaps({-512,1024}),"Damage trigger must remain walkable");
    std::cout<<"PASS: trigger loading, timing, repeat damage, exit/reentry, independent timers, death, walkability\n";
}
