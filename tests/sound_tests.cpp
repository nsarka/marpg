#include "client/sound_system.hpp"
#include "common/damage.hpp"
#include <stdexcept>
#include <iostream>
void check(bool value,const char* message) {if (!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc==2,"Sound pack required");
    SoundSystem sounds(argv[1]);
    const std::array<std::size_t,5> counts{8,10,4,10,4};
    for (std::size_t i=0;i<counts.size();++i)
        check(sounds.variantCount(static_cast<SoundEffect>(i))==counts[i],"All applicable WAV variants must decode");
    SoundEvents events;
    std::vector<common::PlayerState> players(2);
    auto& player=players[0]; player.connected=true;
    common::applyDamage(player,2,-1,player.pos);
    check(events.observe(players).empty(),"Joining must not replay old damage");
    player.lastAttack=common::AttackKind::Jab; ++player.attackSequence;
    auto cues=events.observe(players);
    check(cues.size()==1 && cues[0].effect==SoundEffect::Swing,"A swing must play even without a hit");
    check(events.observe(players).empty(),"Repeated snapshots must not replay attacks");
    common::applyDamage(player,2,-1,player.pos);
    common::applyDamage(player,20,1,player.pos);
    common::applyDamage(player,35,1,player.pos);
    cues=events.observe(players);
    check(cues.size()==3 && cues[0].effect==SoundEffect::SmallDamage &&
          cues[1].effect==SoundEffect::Punch && cues[2].effect==SoundEffect::Punch,
          "Batched damage must play each applicable effect once");
    check(events.observe(players).empty(),"Damage history must be deduplicated");
    common::applyDamage(player,100,1,player.pos);
    cues=events.observe(players);
    check(cues.size()==2 && cues[0].effect==SoundEffect::Punch && cues[1].effect==SoundEffect::Death,
          "Lethal punches must play contact and death cues");
    check(events.observe(players).empty(),"Death must not loop");
    player.alive=true;player.health=100;
    cues=events.observe(players);
    check(cues.size()==1 && cues[0].effect==SoundEffect::Respawn,"Respawn cue missing");
    player.connected=false; events.observe(players); player.connected=true;
    check(events.observe(players).empty(),"Reconnect must reset baseline");
    std::cout<<"PASS: 36 sound assets decoded; attack, damage, death, respawn and snapshot deduplication\n";
}
