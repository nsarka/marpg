#pragma once
#include "common.hpp"
#include "attack_delivery.hpp"
#include "trigger_system.hpp"
#include <array>

namespace common {
class KillHistory {
public:
    void observe(const std::vector<PlayerState*>& players,const TriggerSystem& hazards) {
        for(std::size_t victim=0;victim<players.size() && victim<seen_.size();++victim) {
            auto& player=*players[victim];
            const bool changed=sequenceNewer(player.damageSequence,seen_[victim]);
            seen_[victim]=player.damageSequence;
            if(!changed || !player.connected || player.alive || player.health>0 || player.damageEvents.empty())continue;
            ++player.deaths;
            const auto& damage=player.damageEvents.back();
            if(damage.source>=0 && std::size_t(damage.source)<players.size() &&
               std::size_t(damage.source)!=victim && players[damage.source]->connected)
                ++players[damage.source]->kills;
            KillEvent event;
            event.sequence=++sequence_;event.victim=static_cast<PlayerId>(victim);
            event.victimName=player.name;event.victimTeam=player.team;
            event.killer=damage.source;
            if(damage.source>=0 && std::size_t(damage.source)<players.size()) {
                const auto& killer=*players[damage.source];
                event.killerName=killer.name;event.killerTeam=killer.team;
                event.cause=killer.lastAttack==AttackKind::Jab?KillCause::Jab:
                            killer.lastAttack==AttackKind::Hook?KillCause::Hook:KillCause::Hit;
            }else{
                event.killer=-1;event.killerName="WORLD";
                event.cause=hazards.hasFloor(player.pos)?KillCause::Floor:KillCause::Bounds;
            }
            if(events_.size()==KillHistorySize)events_.erase(events_.begin());
            events_.push_back(std::move(event));
        }
    }
    const std::vector<KillEvent>& events() const{return events_;}
private:
    std::uint32_t sequence_=0;
    std::array<std::uint32_t,MAX_PLAYERS> seen_{};
    std::vector<KillEvent> events_;
};
}
