#pragma once
#include "common.hpp"
#include <SFML/System/String.hpp>
#include <algorithm>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace common {
inline constexpr const char* MSG_CHAT_SEND = "chat_send";
inline constexpr const char* MSG_CHAT_SEND_ACK = "chat_send_ack";
inline constexpr const char* MSG_CHAT = "chat";
inline constexpr const char* MSG_CHAT_ACK = "chat_ack";
inline constexpr std::size_t ChatMaxBytes = 512;
inline constexpr std::size_t ChatMaxCharacters = 160;

inline std::string cleanChatText(const std::string& input) {
    if (input.size() > ChatMaxBytes)
        return {};
    sf::String text = sf::String::fromUtf8(input.begin(), input.end()), clean;
    for (auto c : text) {
        if (c >= 32 && !(c >= 127 && c <= 159) && c != 0x2028 && c != 0x2029 &&
            !(c >= 0x202a && c <= 0x202e) && !(c >= 0x2066 && c <= 0x2069))
            clean += c;
        if (clean.getSize() >= ChatMaxCharacters)
            break;
    }
    while (!clean.isEmpty() && clean[0] == ' ')
        clean.erase(0, 1);
    while (!clean.isEmpty() && clean[clean.getSize() - 1] == ' ')
        clean.erase(clean.getSize() - 1, 1);
    const auto bytes = clean.toUtf8();
    return std::string(bytes.begin(), bytes.end());
}
enum class ChatKind : std::uint8_t { Player, Server, Announcement, System };
struct ChatMessage {
    std::uint32_t sequence = 0;
    PlayerId sender = 0;
    std::int32_t team = -1;
    std::uint32_t deaths = 0;
    bool dead = false;
    std::string name, text;
    ChatKind kind = ChatKind::Player;
};
inline ChatMessage playerChat(std::uint32_t sequence, PlayerId id, const PlayerState& state,
                              const std::string& text) {
    return {sequence,
            id,
            state.team,
            state.deaths,
            !state.alive || state.health <= 0,
            cleanChatText(state.name),
            cleanChatText(text)};
}
inline sf::Packet chatPacket(const ChatMessage& message) {
    sf::Packet packet;
    packet << std::string(MSG_CHAT) << message.sequence << message.sender << message.team << message.deaths
           << message.dead << message.name << message.text << static_cast<std::uint8_t>(message.kind);
    return packet;
}
inline bool readChat(sf::Packet& packet, ChatMessage& message) {
    std::uint8_t kind;
    if (!(packet >> message.sequence >> message.sender >> message.team >> message.deaths >> message.dead >>
          message.name >> message.text >> kind) ||
        kind > static_cast<std::uint8_t>(ChatKind::System))
        return false;
    message.kind = static_cast<ChatKind>(kind);
    return (message.kind != ChatKind::Player || message.sender < MAX_PLAYERS) && message.name.size() <= 64 &&
           !message.text.empty() && message.text == cleanChatText(message.text) &&
           message.name == cleanChatText(message.name);
}
inline std::string chatLine(const ChatMessage& message) {
    if (message.kind == ChatKind::System)
        return message.text;
    if (message.kind != ChatKind::Player)
        return "Server: " + message.text;
    return (message.dead ? "*DEAD* " : "") + message.name + ": " + message.text;
}
// A window of IDs tolerates reordering while displaying retransmissions once.
class ChatInbox {
    std::deque<std::uint32_t> seen_;

  public:
    bool accept(std::uint32_t sequence) {
        if (std::find(seen_.begin(), seen_.end(), sequence) != seen_.end())
            return false;
        seen_.push_back(sequence);
        if (seen_.size() > 256)
            seen_.pop_front();
        return true;
    }
};
// Each hop retries independently. Queues are bounded; a broken connection cannot
// retain unlimited messages. Times are milliseconds from the owning connection.
class ChatOutbox {
    struct Pending {
        std::uint32_t sequence;
        sf::Packet packet;
        std::int64_t created, lastSent;
    };
    std::deque<Pending> pending_;

  public:
    bool enqueue(std::uint32_t sequence, sf::Packet packet, std::int64_t now) {
        if (pending_.size() >= 64)
            return false;
        pending_.push_back({sequence, std::move(packet), now, now - 250});
        return true;
    }
    void acknowledge(std::uint32_t sequence) {
        pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                      [&](const auto& p) { return p.sequence == sequence; }),
                       pending_.end());
    }
    std::size_t expire(std::int64_t now) {
        const auto before = pending_.size();
        pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                      [&](const auto& p) { return now - p.created >= 15000; }),
                       pending_.end());
        return before - pending_.size();
    }
    std::vector<sf::Packet> due(std::int64_t now) {
        std::vector<sf::Packet> packets;
        for (auto& p : pending_)
            if (now - p.lastSent >= 250) {
                p.lastSent = now;
                packets.push_back(p.packet);
            }
        return packets;
    }
};
} // namespace common
