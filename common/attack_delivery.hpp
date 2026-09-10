#pragma once
#include "common.hpp"
#include <algorithm>
#include <deque>
#include <optional>

namespace common {
inline constexpr const char* MSG_ATTACK = "attack";
inline constexpr const char* MSG_ATTACK_ACK = "attack_ack";
inline constexpr std::uint32_t AttackBufferMs = 250;
inline constexpr Tick AttackBufferTicks = TICK_RATE / 4;
struct AttackRequest {
    std::uint32_t sequence = 0;
    AttackKind kind = AttackKind::None;
    sf::Vector2f aim{};
    std::uint32_t ageMs = 0;
};
inline sf::Packet attackPacket(PlayerId player, const AttackRequest& request) {
    sf::Packet packet;
    packet << std::string(MSG_ATTACK) << player << request.sequence << static_cast<std::uint8_t>(request.kind)
           << request.aim.x << request.aim.y << request.ageMs;
    return packet;
}
inline bool readAttackRequest(sf::Packet& packet, PlayerId& player, AttackRequest& request) {
    std::uint8_t kind = 0;
    if (!(packet >> player >> request.sequence >> kind >> request.aim.x >> request.aim.y >> request.ageMs))
        return false;
    request.kind = static_cast<AttackKind>(kind);
    return true;
}
inline bool sequenceNewer(std::uint32_t sequence, std::uint32_t previous) {
    const auto delta = sequence - previous;
    return delta != 0 && delta < 0x80000000u;
}
// Attack IDs are independent of movement sequence numbers. Old/reordered requests
// are acknowledged but never replace a newer click or restart a consumed swing.
class AttackInbox {
  public:
    bool accept(std::uint32_t sequence) {
        if (latest_ && !sequenceNewer(sequence, *latest_))
            return false;
        latest_ = sequence;
        return true;
    }

  private:
    std::optional<std::uint32_t> latest_;
};
class AttackOutbox {
  public:
    void enqueue(AttackKind kind, sf::Vector2f aim, std::uint32_t nowMs) {
        // Bound memory during a disconnect; these old clicks are already unusable.
        if (pending_.size() == 64)
            pending_.pop_front();
        pending_.push_back({AttackRequest{next_++, kind, aim, 0}, nowMs});
    }
    std::vector<AttackRequest> requests(std::uint32_t nowMs) const {
        std::vector<AttackRequest> result;
        for (const auto& pending : pending_) {
            auto request = pending.request;
            request.ageMs = nowMs - pending.createdMs;
            result.push_back(request);
        }
        return result;
    }
    void acknowledge(std::uint32_t sequence) {
        pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                      [&](const auto& p) { return p.request.sequence == sequence; }),
                       pending_.end());
    }
    void clear() {
        pending_.clear();
    }

  private:
    struct Pending {
        AttackRequest request;
        std::uint32_t createdMs;
    };
    std::deque<Pending> pending_;
    std::uint32_t next_ = 0;
};
} // namespace common
