#include "common/combat_system.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    common::CollisionWorld walls;
    common::PlayerState attacker,target,other;
    attacker.connected=target.connected=other.connected=true;
    attacker.pos={0,0};target.pos={30,0};other.pos={50,0};
    common::CombatState combat;combat.facing={1,0};
    std::vector<common::PlayerState*> targets{&attacker,&target,&other};
    check(common::startAttack(attacker,combat,common::AttackKind::Jab),"Jab must start");
    check(!common::startAttack(attacker,combat,common::AttackKind::Hook),"No attack during cooldown");
    for(int i=0;i<15;++i)common::updateAttack(attacker,combat,targets,walls);
    check(target.health==100,"No damage during windup");
    common::updateAttack(attacker,combat,targets,walls);
    check(target.health==80 && other.health==100,"Jab hits nearest target once for 20");
    for(int i=0;i<48;++i)common::updateAttack(attacker,combat,targets,walls);
    check(target.health==80 && combat.attack==common::AttackKind::None,"One hit per swing, cooldown completes");
    common::startAttack(attacker,combat,common::AttackKind::Hook);
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,targets,walls);
    check(target.health==45,"Hook does 35 damage");
    target.pos={-30,0};other.pos={1000,0};
    common::startAttack(attacker,combat,common::AttackKind::Jab);
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,targets,walls);
    check(target.health==45 && other.health==100,"No backward or out-of-range hits");
    target.pos={30,0};
    walls.addPolygon({{10,-50},{15,-50},{15,50},{10,50}});
    common::startAttack(attacker,combat,common::AttackKind::Hook);
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,targets,walls);
    check(target.health==45,"Cannot attack through walls");
    target.health=1; common::CollisionWorld open;
    common::startAttack(attacker,combat,common::AttackKind::Jab);
    for(int i=0;i<16;++i)common::updateAttack(attacker,combat,targets,open);
    check(target.health==0 && !target.alive,"Lethal damage kills and clamps health");
    common::CombatState victim;
    check(!common::startAttack(target,victim,common::AttackKind::Jab),"Dead players cannot attack");
    for(unsigned i=0;i<common::RespawnDelayTicks-1;++i)
        check(!common::advanceDeath(target,victim),"Death animation must finish before respawn");
    check(common::advanceDeath(target,victim),"Respawn must become ready after 2.5 seconds");
    auto name=target.name;
    common::respawn(target,victim,{-140,620});
    check(target.health==100 && target.alive && target.pos==sf::Vector2f(-140,620) && target.name==name,"Respawn restores health, position and identity");
    check(common::startAttack(target,victim,common::AttackKind::Hook),"Respawned player can attack");
    target.health=-4;
    common::advanceDeath(target,victim);
    check(!target.alive && target.health==0,"Negative health also dies");
    common::PlayerState network;
    network.combatDebug={common::AttackKind::Hook,24,{1.f,0.f},true,3};
    sf::Packet packet;
    common::writePlayerState(packet,network);
    common::PlayerState decoded;
    check(common::readPlayerState(packet,decoded),"Combat debug snapshot decoding");
    check(decoded.combatDebug.attack==common::AttackKind::Hook && decoded.combatDebug.age==24 &&
          decoded.combatDebug.direction==sf::Vector2f(1,0) && decoded.combatDebug.hit && decoded.combatDebug.target==3,
          "Combat debug fields must round-trip accurately");
    check(combat.hitTarget==1,"Confirmed hit identifies the selected target");
    common::PlayerState mouseAttacker, mouseTarget;
    mouseAttacker.connected=mouseTarget.connected=true;
    mouseAttacker.pos={0,0}; mouseTarget.pos={30,0};
    common::CombatState mouseCombat; mouseCombat.facing={-1,0};
    check(common::startAttack(mouseAttacker,mouseCombat,common::AttackKind::Jab,{10,0}),"Mouse-aimed attack starts");
    check(mouseCombat.attackDirection==sf::Vector2f(1,0),"Mouse aim overrides movement direction and normalizes");
    std::vector<common::PlayerState*> mouseTargets{&mouseAttacker,&mouseTarget};
    mouseCombat.facing={0,1};
    for(int i=0;i<16;++i)common::updateAttack(mouseAttacker,mouseCombat,mouseTargets,open);
    check(mouseTarget.health==80,"Hit follows captured mouse direction despite movement");
    common::CombatState invalid;
    check(!common::startAttack(mouseAttacker,invalid,common::AttackKind::Hook,{std::numeric_limits<float>::quiet_NaN(),0}),"Reject invalid aim");
    invalid.facing={0,1};
    check(common::startAttack(mouseAttacker,invalid,common::AttackKind::Hook,{}),"Cursor at feet has facing fallback");
    check(invalid.attackDirection==sf::Vector2f(0,1),"Zero-length aim fallback");
    common::InputCommand input; input.aim={-0.6f,0.8f}; input.hookPressed=true;
    sf::Packet inputPacket; common::writeInputCmd(inputPacket,7,input);
    std::string type;inputPacket>>type;
    common::InputCommand received;common::PlayerId id;
    check(common::readInputCmd(inputPacket,id,received) && id==7 && received.aim==input.aim && received.hookPressed,
          "Mouse aim must round-trip through network input");
    common::PlayerState buffered;
    buffered.connected=true;
    common::CombatState bufferCombat;
    common::startAttack(buffered,bufferCombat,common::AttackKind::Jab,{1,0});
    std::vector<common::PlayerState*> noTargets;
    for(int i=0;i<55;++i) common::updateAttack(buffered,bufferCombat,noTargets,open);
    check(common::requestAttack(buffered,bufferCombat,{1,common::AttackKind::Hook,{0,1},0}),"Late click is buffered");
    for(int i=0;i<9;++i) common::updateAttack(buffered,bufferCombat,noTargets,open);
    check(buffered.attackSequence==2 && bufferCombat.attack==common::AttackKind::Hook &&
          bufferCombat.attackDirection==sf::Vector2f(0,1),"Buffered attack starts at recovery end with original aim");
    common::requestAttack(buffered,bufferCombat,{2,common::AttackKind::Jab,{1,0},0});
    for(int i=0;i<64;++i) common::updateAttack(buffered,bufferCombat,noTargets,open);
    check(buffered.attackSequence==2 && bufferCombat.bufferedAttack==common::AttackKind::None,"Early clicks expire rather than replay later");
    check(!common::requestAttack(buffered,bufferCombat,{3,common::AttackKind::Jab,{1,0},250}),"Expired network input cannot start a surprise attack");
    common::AttackOutbox outbox;
    outbox.enqueue(common::AttackKind::Jab,{0,1},100);
    auto first=outbox.requests(100), retry=outbox.requests(150);
    check(first.size()==1 && retry.size()==1 && first[0].sequence==retry[0].sequence && retry[0].ageMs==50,
          "Lost input is retried with the same ID, aim and original age");
    common::AttackInbox inbox;
    check(inbox.accept(retry[0].sequence),"Retried input accepted once");
    check(!inbox.accept(retry[0].sequence),"Lost ACK retry cannot duplicate attack or refresh buffer");
    outbox.enqueue(common::AttackKind::Hook,{1,0},160);
    auto requests=outbox.requests(170);
    outbox.acknowledge(requests[1].sequence);
    check(outbox.requests(180).size()==1,"Out-of-order ACK only removes its own request");
    outbox.acknowledge(first[0].sequence);
    check(outbox.requests(190).empty(),"ACK stops retries");
    common::AttackInbox wrapping;
    check(wrapping.accept(0xffffffffu) && wrapping.accept(0) && !wrapping.accept(0xffffffffu),"Attack IDs support wrap and reject stale delivery");
    auto attackPacket=common::attackPacket(9,retry[0]);
    attackPacket>>type;
    common::AttackRequest decodedRequest;
    check(common::readAttackRequest(attackPacket,id,decodedRequest) && id==9 && decodedRequest.ageMs==50 &&
          decodedRequest.aim==sf::Vector2f(0,1),"Reliable attack packet round trip");
    check(common::inAttackArc({30,29},{1,0},70),"Target just inside narrowed cone");
    check(!common::inAttackArc({30,31},{1,0},70),"Target outside 90-degree cone misses");
    check(target.damageEvents.back().amount==1 && target.damageEvents.back().source==0,
          "Overkill event records actual damage and source");
    sf::Packet damagePacket; common::writePlayerState(damagePacket,target);
    common::PlayerState damageDecoded;
    check(common::readPlayerState(damagePacket,damageDecoded) &&
          damageDecoded.damageEvents.back().amount==1 &&
          damageDecoded.damageEvents.back().contact==target.damageEvents.back().contact,
          "Damage event contact and amount survive serialization");
    check((100+common::attackDescription(common::AttackKind::Jab).damage-1)/common::attackDescription(common::AttackKind::Jab).damage==5,
          "Five jabs should defeat a full-health player");
    check((100+common::attackDescription(common::AttackKind::Hook).damage-1)/common::attackDescription(common::AttackKind::Hook).damage==3,
          "Three hooks should defeat a full-health player");
    {
        common::activeSettings.spell.damageMin=common::activeSettings.spell.damageMax=30;
        common::PlayerState caster,a,b,friendPlayer,outside;
        caster.connected=a.connected=b.connected=friendPlayer.connected=outside.connected=true;
        caster.pos={0,0};caster.team=friendPlayer.team=0;a.team=b.team=outside.team=1;
        a.pos={200,0};b.pos={300,0};friendPlayer.pos={210,0};outside.pos={321,0};
        common::CombatState cast;common::CollisionWorld empty;
        std::vector<common::PlayerState*> targets{&caster,&a,&b,&friendPlayer,&outside};
        check(!common::startAttack(caster,cast,common::AttackKind::Uppercut,{501,0}),"Spell range must be enforced");
        check(common::startAttack(caster,cast,common::AttackKind::Uppercut,{200,0}),"Spell did not start");
        for(int i=0;i<23;++i)common::updateAttack(caster,cast,targets,empty);
        check(a.health==100,"Spell hit before windup");
        common::updateAttack(caster,cast,targets,empty);
        check(a.health==70 && b.health==70 && friendPlayer.health==100 && outside.health==100,"AoE radius or teams incorrect");
        check(caster.spellSequence==1 && caster.spellPosition==sf::Vector2f(200,0) && a.damageEvents.back().source==0,"Spell effect or hurt attribution missing");
        check(!common::startAttack(caster,cast,common::AttackKind::Uppercut,{200,0}),"Spell cooldown bypassed");
        for(int i=24;i<129;++i)common::updateAttack(caster,cast,targets,empty);
        check(cast.attack==common::AttackKind::None && a.health==70,"Spell repeated or cooldown never ended");
        caster.pos={200,0};
        check(common::startAttack(caster,cast,common::AttackKind::Uppercut,{200,0}),"Self-area cast failed");
        for(int i=0;i<24;++i)common::updateAttack(caster,cast,targets,empty);
        check(caster.health==70 && a.health==40 && friendPlayer.health==100,"Spell must hurt caster and enemies but spare teammates");
        check(caster.damageEvents.back().source==0,"Self damage attribution lost");
        cast={};common::activeSettings.friendlyFire=true;
        common::startAttack(caster,cast,common::AttackKind::Uppercut,{200,0});
        for(int i=0;i<24;++i)common::updateAttack(caster,cast,targets,empty);
        check(caster.health==40 && friendPlayer.health==70,"Friendly fire should allow teammate damage without disabling self damage");
        common::activeSettings.friendlyFire=false;
        std::vector<common::PlayerState> states(common::MAX_PLAYERS);states[0]=caster;
        sf::Packet packet;common::writeWorldPacket(packet,states);std::string type;packet>>type;
        std::vector<common::PlayerState> result;
        check(common::readWorldPacket(packet,result) && result[0].spellSequence==3 && result[0].spellPosition==caster.spellPosition,"Spell snapshot did not roundtrip");
    }
    common::applySettings(common::ServerSettings{});
    {
        common::PlayerState caster,victim,ally;caster.connected=victim.connected=ally.connected=true;
        caster.pos={0,0};victim.pos=ally.pos={200,0};caster.team=ally.team=0;victim.team=1;
        common::CombatState cast;common::CollisionWorld empty;
        std::vector<common::PlayerState*> targets{&caster,&victim,&ally};
        check(common::requestAttack(caster,cast,{0,common::AttackKind::Lightning,{200,0},0}),"Lightning request rejected");
        for(int i=0;i<15;++i)common::updateAttack(caster,cast,targets,empty);
        check(victim.health==100,"Lightning hit before windup");
        common::updateAttack(caster,cast,targets,empty);
        check(victim.health>=96 && victim.health<=99 && caster.spellSequence==1,"First lightning tick missing");
        for(int i=0;i<21;++i)common::updateAttack(caster,cast,targets,empty);
        check(caster.spellSequence==1,"Lightning hit before one third second");
        common::updateAttack(caster,cast,targets,empty);check(caster.spellSequence==2,"Lightning repeat missing");
        for(int i=38;i<272;++i)common::updateAttack(caster,cast,targets,empty);
        check(caster.spellSequence==9 && cast.attack==common::AttackKind::None,"Lightning duration/cooldown incorrect");
        check(ally.health==100 && victim.health>=64 && victim.health<=91,"Lightning damage bounds or team filter wrong");
        for(const auto& hit:victim.damageEvents)check(hit.amount>=1 && hit.amount<=4,"Lightning damage outside configured range");
        check(caster.spellEffect==common::AttackKind::Lightning,"Lightning visual not tagged");
        common::activeSettings.spell.radius=25;common::activeSettings.spell.range=250;
        common::activeSettings.spell.damageMin=5;common::activeSettings.spell.damageMax=30;
        caster.pos={0,0};victim.pos={200,0};
        for(int trial=0;trial<32;++trial) {
            victim.health=100;victim.alive=true;cast={};
            common::startAttack(caster,cast,common::AttackKind::Uppercut,{200,0});
            for(int i=0;i<24;++i)common::updateAttack(caster,cast,targets,empty);
            check(victim.health>=70 && victim.health<=95,"Explosion damage outside configured range");
        }
    }
    common::applySettings(common::ServerSettings{});
    std::cout<<"PASS: jab/hook damage, windup, cooldown, range, facing, walls, death and respawn\n";
}
