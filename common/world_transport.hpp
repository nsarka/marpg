#pragma once
#include "common.hpp"
#include "attack_delivery.hpp"
#include <map>
#include <optional>
#include <cstring>
#include <stdexcept>

namespace common {
inline constexpr const char* MSG_WORLD_PART="world_part";
inline constexpr std::size_t WorldPartBytes=1000;
inline constexpr std::size_t MaxWorldBytes=65536;
inline std::vector<sf::Packet> worldPackets(const std::vector<PlayerState>& players,std::uint32_t sequence,const std::vector<KillEvent>& kills={}) {
    sf::Packet world; writeWorldPacket(world,players,kills);
    const auto size=world.getDataSize();
    if (size>MaxWorldBytes) throw std::runtime_error("World snapshot exceeds transport limit");
    const auto count=static_cast<std::uint16_t>((size+WorldPartBytes-1)/WorldPartBytes);
    std::vector<sf::Packet> packets(count);
    for (std::uint16_t i=0;i<count;++i) {
        packets[i] << std::string(MSG_WORLD_PART) << sequence << i << count << static_cast<std::uint32_t>(size);
        const auto offset=i*WorldPartBytes;
        packets[i].append(static_cast<const char*>(world.getData())+offset,std::min(WorldPartBytes,size-offset));
    }
    return packets;
}
class WorldAssembler {
public:
    // Caller has already read the message type and validated the sender.
    std::optional<sf::Packet> accept(sf::Packet& packet) {
        std::uint32_t sequence,size;
        std::uint16_t index,count;
        if (!(packet >> sequence >> index >> count >> size) || size==0 || size>MaxWorldBytes ||
            count!=(size+WorldPartBytes-1)/WorldPartBytes || index>=count) return std::nullopt;
        if (latest_ && !sequenceNewer(sequence,*latest_)) return std::nullopt;
        const auto offset=index*WorldPartBytes;
        const auto length=std::min(WorldPartBytes,size-offset);
        if (packet.getDataSize()-packet.getReadPosition()!=length) return std::nullopt;
        auto it=pending_.find(sequence);
        if (it==pending_.end()) {
            // Bound incomplete snapshots even when packets are lost or reordered.
            if (pending_.size()>=4) {
                auto oldest=pending_.begin();
                for (auto entry=pending_.begin();entry!=pending_.end();++entry)
                    if (sequenceNewer(oldest->first,entry->first)) oldest=entry;
                if (!sequenceNewer(sequence,oldest->first)) return std::nullopt;
                pending_.erase(oldest);
            }
            it=pending_.emplace(sequence,Assembly{std::vector<char>(size),std::vector<bool>(count),0}).first;
        }
        auto& assembly=it->second;
        if (assembly.bytes.size()!=size) return std::nullopt;
        if (!assembly.received[index]) {
            std::memcpy(assembly.bytes.data()+offset,static_cast<const char*>(packet.getData())+packet.getReadPosition(),length);
            assembly.received[index]=true; ++assembly.count;
        }
        if (assembly.count!=count) return std::nullopt;
        sf::Packet result; result.append(assembly.bytes.data(),assembly.bytes.size());
        latest_=sequence;
        for (auto entry=pending_.begin();entry!=pending_.end();)
            if (!sequenceNewer(entry->first,sequence)) entry=pending_.erase(entry); else ++entry;
        return result;
    }
private:
    struct Assembly {std::vector<char> bytes;std::vector<bool> received;std::size_t count;};
    std::map<std::uint32_t,Assembly> pending_;
    std::optional<std::uint32_t> latest_;
};
}
