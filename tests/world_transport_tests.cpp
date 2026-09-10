#include "common/character_roster.hpp"
#include "common/world_transport.hpp"
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::optional<sf::Packet> deliver(common::WorldAssembler& receiver,sf::Packet packet){
    std::string type;packet>>type;check(type==common::MSG_WORLD_PART,"Wrong envelope");
    return receiver.accept(packet);
}
int main(){
    std::vector<common::PlayerState> players(common::MAX_PLAYERS);
    for(auto& player:players){
        player.connected=true;player.name=std::string(64,'x');
        for(unsigned i=1;i<=8;++i)player.damageEvents.push_back({i,2,-1,{100,200}});
    }
    std::mt19937 random(42);
    std::array<bool,7> seen{};
    for(int i=0;i<200;++i)for(int team=0;team<2;++team) {
        auto character=common::chooseCharacter(team,random);seen[character]=true;
        check(team==0?character<3:character>=3 && character<7,"Character escaped team pool");
    }
    for(bool available:seen)check(available,"Character variant is never selected");
    for(std::size_t i=0;i<players.size();++i)players[i].character=i%7;
    auto parts=common::worldPackets(players,1);
    check(parts.size()>1,"Test must exercise multiple datagrams");
    for(const auto& part:parts)check(part.getDataSize()<=1200,"Datagram exceeds safe budget");
    common::WorldAssembler receiver;
    check(!deliver(receiver,parts.back()),"Partial snapshot published");
    check(!deliver(receiver,parts.back()),"Duplicate completed snapshot");
    std::optional<sf::Packet> result;
    for(std::size_t i=parts.size()-1;i-->0;)result=deliver(receiver,parts[i]);
    check(result.has_value(),"Reordered snapshot not assembled");
    std::string type;*result>>type;
    std::vector<common::PlayerState> decoded;
    check(type==common::MSG_WORLD && common::readWorldPacket(*result,decoded),"Snapshot decode failed");
    for(std::size_t i=0;i<players.size();++i)check(decoded[i].character==players[i].character,"Character selection lost in snapshot");
    check(decoded.size()==32 && decoded.back().damageEvents.size()==8 && decoded.back().name==players.back().name,"Roundtrip changed state");
    for(auto part:parts)check(!deliver(receiver,part),"Stale snapshot replayed");
    auto lost=common::worldPackets(players,2);
    for(std::size_t i=1;i<lost.size();++i)check(!deliver(receiver,lost[i]),"Missing part accepted");
    auto fresh=common::worldPackets(players,3);
    for(auto part:fresh)result=deliver(receiver,part);
    check(result.has_value(),"Dropped old snapshot blocked fresh one");
    check(!deliver(receiver,lost[0]),"Late old snapshot rolled state back");
    sf::Packet malformed=parts[0];malformed.append("x",1);
    common::WorldAssembler invalid;
    check(!deliver(invalid,malformed),"Malformed payload accepted");
    sf::Packet truncated;truncated<<true;
    auto previous=decoded;
    check(!common::readWorldPacket(truncated,decoded) && decoded[0].name==previous[0].name,"Partial decode mutated world");
    std::cout<<"PASS: bounded datagrams, 32-player roundtrip, reordering, duplicates, loss recovery, stale and malformed packets\n";
}
