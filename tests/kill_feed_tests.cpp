#include "client/kill_feed.hpp"
#include "client/scoreboard.hpp"
#include "client/attack_hud.hpp"
#include "client/spell_effects.hpp"
#include "common/kill_history.hpp"
#include "common/damage.hpp"
#include "common/world_transport.hpp"
#include <filesystem>
#include <iostream>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==2,"Project path required");std::filesystem::path root=argv[1];
    common::TriggerSystem hazards;hazards.load((root/"assets/tiled/Demo.tmx").string());
    std::vector<common::PlayerState> players(common::MAX_PLAYERS);auto& killer=players[0];auto& victim=players[1];
    killer.connected=victim.connected=true;killer.name="Alice";victim.name="Bob";killer.team=0;victim.team=1;killer.lastAttack=common::AttackKind::Hook;
    common::KillHistory history;std::vector<common::PlayerState*> references;for(auto& player:players)references.push_back(&player);
    common::applyDamage(victim,100,0,victim.pos);history.observe(references,hazards);
    check(history.events().size()==1 && history.events()[0].cause==common::KillCause::Hook && history.events()[0].killerName=="Alice","Hook kill was not recorded");
    history.observe(references,hazards);check(history.events().size()==1,"Dead player generated duplicate kills");
    check(killer.kills==1 && victim.deaths==1,"Death counted more than once");
    KillFeed feed;feed.observe(history.events(),0);check(feed.entries().empty(),"Join replayed historical kills");
    victim.alive=true;victim.health=100;killer.lastAttack=common::AttackKind::Jab;
    common::applyDamage(victim,100,0,victim.pos);history.observe(references,hazards);feed.observe(history.events(),0);
    check(feed.entries().size()==1 && feed.entries()[0].local && feed.entries()[0].event.cause==common::KillCause::Jab,"New kill missing or not highlighted");
    killer.name="Reused slot";feed.observe(history.events(),0);
    check(feed.entries().size()==1 && feed.entries()[0].event.killerName=="Alice","Name changed after slot reuse or snapshot replay");
    for(auto position:{sf::Vector2f{-512,1024},sf::Vector2f{128,-10}}) {
        victim.alive=true;victim.health=2;victim.pos=position;
        common::applyDamage(victim,2,-1,position);history.observe(references,hazards);
    }
    check(history.events()[2].cause==common::KillCause::Floor && history.events()[3].cause==common::KillCause::Bounds,"Environmental causes incorrect");
    feed.observe(history.events(),0);check(feed.entries().size()==3,"Missed snapshot did not recover kills");
    sf::Packet wire;common::writeWorldPacket(wire,players,history.events());std::string type;wire>>type;
    std::vector<common::PlayerState> received;std::vector<common::KillEvent> kills;
    check(common::readWorldPacket(wire,received,&kills) && kills.size()==4 && kills[0].killerName=="Alice" && kills[3].cause==common::KillCause::Bounds,"Kill history did not roundtrip");
    check(received[0].kills==2 && received[1].deaths==4,"Scoreboard totals did not roundtrip");
    std::vector<common::KillEvent> burst;
    for(unsigned i=100;i<110;++i){auto event=history.events()[0];event.sequence=i;burst.push_back(event);}
    feed.observe(burst,0);check(feed.entries().size()==6 && feed.entries().front().event.sequence==104,"Feed should retain newest six rows");
    feed.update(6.5f);check(KillFeed::opacity(feed.entries()[0])==.5f,"Feed should fade in its final second");
    feed.update(.5f);check(feed.entries().empty(),"Feed did not expire");feed.observe(burst,0);check(feed.entries().empty(),"Expired entries replayed");
    for(unsigned i=0;i<40;++i){victim.alive=true;victim.health=1;common::applyDamage(victim,1,0,victim.pos);history.observe(references,hazards);}
    check(history.events().size()==common::KillHistorySize,"Server kill history must be bounded");
    check(killer.kills==42 && victim.deaths==44,"Totals must outlive bounded kill history and respawns");
    sf::Font font;check(font.openFromFile(root/"assets/fonts/arial.ttf"),"Font missing");
    KillFeed preview;preview.observe({},0);
    common::KillEvent first{1,0,1,0,1,"Rick","Bot 2",common::KillCause::Hook};
    common::KillEvent second{2,1,0,1,0,"Bot 4","Rick",common::KillCause::Jab};
    common::KillEvent third{3,-1,3,-1,1,"WORLD","Bot 8",common::KillCause::Bounds};
    preview.observe({first,second,third},0);preview.update(.2f);
    sf::RenderTexture target({640,240});target.clear(sf::Color(35,40,46));preview.draw(target,font);target.display();
    check(target.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-kill-feed.png"),"Preview save failed");
    sf::RenderTexture board({1000,720});board.clear();drawScoreboard(board,font,players,0);board.display();
    check(board.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-scoreboard.png"),"Scoreboard preview save failed");
    players[0].combatDebug.attack=common::AttackKind::Hook;players[0].combatDebug.age=32;
    check(attackSecondsRemaining(players[0])==.5f,"Cooldown remaining time incorrect");
    drawAttackHud(board,font,players[0]);board.display();
    check(board.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-combat-hud.png"),"HUD preview save failed");
    players[0].combatDebug.attack=common::AttackKind::None;players[0].stunTicks=32;
    check(attackSecondsRemaining(players[0])==.5f,"Idle stun timer must display");
    drawAttackHud(board,font,players[0]);board.display();
    players[0].stunTicks=0;
    players[0].explosionCooldown=64;players[0].lightningCooldown=0;
    check(attackSecondsRemaining(players[0],common::AttackKind::Uppercut)==1.f &&
          attackSecondsRemaining(players[0],common::AttackKind::Lightning)==0,"Spell HUD timers must be independent");
    players[0].explosionCooldown=0;
    SpellEffects spell(root/"assets/sprites/Free Pixel Art Explosions/PNG/Explosion");
    spell.observe(players,0);++players[0].spellSequence;players[0].spellPosition={320,120};spell.observe(players,.2f);spell.draw(target);
    ++players[0].spellSequence;players[0].spellEffect=common::AttackKind::Lightning;spell.observe(players,.05f);spell.draw(target);
    SpellEffects warnings(root/"assets/sprites/Free Pixel Art Explosions/PNG/Explosion");
    players[0].alive=players[1].alive=true;
    players[0].combatDebug.attack=common::AttackKind::None;
    warnings.predictCast(0,players[0],common::AttackKind::Uppercut,{180,150});
    sf::RenderTexture telegraph({640,300});
    const sf::Color background(25,30,40);
    telegraph.clear(background);warnings.drawWindups(telegraph);telegraph.display();
    const auto hasEffect=[&](unsigned left,unsigned right) {
        const auto image=telegraph.getTexture().copyToImage();
        for(unsigned y=0;y<300;++y)for(unsigned x=left;x<right;++x)
            if(image.getPixel({x,y})!=background)return true;
        return false;
    };
    check(hasEffect(0,320),"Click must immediately show spell warning");
    players[0].combatDebug={common::AttackKind::Uppercut,12};++players[0].attackSequence;players[0].spellPosition={180,150};
    players[1].combatDebug={common::AttackKind::Lightning,8};players[1].spellPosition={470,150};
    warnings.observe(players,0);
    telegraph.clear(background);warnings.drawWindups(telegraph);telegraph.display();
    check(hasEffect(320,640),"Remote lightning windup warning missing");
    check(telegraph.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-spell-windups.png"),"Warning preview save failed");
    players[1].combatDebug.age=common::attackDescription(common::AttackKind::Lightning).startupTicks+10;
    warnings.observe(players,.2f);
    telegraph.clear(background);warnings.drawWindups(telegraph);telegraph.display();
    check(hasEffect(320,640),"Lightning spot must persist through active spell");
    players[1].combatDebug.age=common::attackDescription(common::AttackKind::Lightning).startupTicks+
        common::attackDescription(common::AttackKind::Lightning).activeTicks;
    warnings.observe(players,0);
    telegraph.clear(background);warnings.drawWindups(telegraph);telegraph.display();
    check(!hasEffect(320,640),"Lightning spot must end before cooldown");
    players[0].combatDebug.attack=players[1].combatDebug.attack=common::AttackKind::None;
    warnings.observe(players,0);
    telegraph.clear(background);warnings.drawWindups(telegraph);telegraph.display();
    check(!hasEffect(0,640),"Interrupted warnings must disappear");
    std::cout<<"PASS: kill attribution, stable names, environment deaths, network history, deduplication, burst cap, fade, and rendering\n";
}
