#include "common/common.hpp"
#include "common/world_transport.hpp"
#include "common/collision_world.hpp"
#include "common/trigger_system.hpp"
#include "common/combat_system.hpp"
#include "common/logger.hpp"

#include <SFML/Network.hpp>

#include <cstdlib>
#include <ctime>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct ServerPlayer {
    common::PlayerState state;
    std::optional<sf::IpAddress> ip;
    unsigned short port = 0;
    std::optional<std::uint32_t> lastInputSequence;
    common::CombatState combat;
    common::AttackInbox attackInbox;
    sf::Vector2f requestedVelocity{};
    sf::Clock lastInputTime;
    sf::Clock lastHeard;
};

namespace {

bool sendPacket(sf::UdpSocket& socket, sf::Packet& packet, const sf::IpAddress& ip, unsigned short port, const char* context) {
    const sf::Socket::Status status = socket.send(packet, ip, port);
    if (status != sf::Socket::Status::Done) {
        std::cerr << context << " failed with socket status "
                  << static_cast<int>(status) << '\n';
        return false;
    }
    return true;
}

std::optional<sf::Vector2f> findSpawn(const std::vector<ServerPlayer>& players,
    const common::CollisionWorld& collision, const common::TriggerSystem& triggers) {
    const auto preferred = common::PlayerState{}.pos;
    for (int ring=0; ring<32; ++ring) {
        for (int step=0; step<16; ++step) {
            const float angle=step*6.2831853f/16.f;
            const auto candidate=preferred+sf::Vector2f{std::cos(angle)*ring*24.f,std::sin(angle)*ring*24.f};
            if (collision.overlaps(candidate) || triggers.contains(candidate)) continue;
            bool occupied=false;
            for (const auto& other : players) {
                if (other.state.connected && other.state.alive &&
                    (other.state.pos-candidate).length()<2.f*common::CollisionWorld::PlayerRadius+1.f)
                    occupied=true;
            }
            if (!occupied) return candidate;
        }
    }
    return std::nullopt;
}

common::PlayerId botId = 0;
common::PlayerId botId2 = 1;
void initializeBot(std::vector<ServerPlayer>& players, common::PlayerId bid) {
    players[bid].state.alive = true;
    players[bid].state.connected = true;
    players[bid].state.pos.x = 60.f * bid;
    players[bid].state.pos.y = 550.f;
    players[bid].state.name = "Bot";
}

} // namespace

int main() {
    common::Logger logger;

    logger.info() << "Server started";

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    common::CollisionWorld collision;
    common::TriggerSystem triggers;
    try {
        collision.load(common::LEVEL_PATH);
        triggers.load(common::LEVEL_PATH);
    } catch (const std::exception& error) {
        logger.log_error(error.what());
        return 1;
    }
    logger.log_info("Loaded collision polygons: ", collision.size());

    sf::UdpSocket socket;
    if (socket.bind(common::SERVER_PORT) != sf::Socket::Status::Done) {
        logger.log_error("Failed to bind server socket on port ", common::SERVER_PORT);
        return 1;
    }
    socket.setBlocking(false);

    std::vector<ServerPlayer> players(common::MAX_PLAYERS);
    initializeBot(players, 0);
    logger.log_info("Initialized bot 0 state to ", players[0].state);
    initializeBot(players, 1);
    logger.log_info("Initialized bot 1 state to ", players[1].state);

    logger.log_info("Server listening on port ", common::SERVER_PORT);

    const auto disconnect=[&](common::PlayerId id) {
        auto& player=players[id];
        logger.log_info("Player disconnected: ",id," (",player.state.name,")");
        const auto attackSequence=player.state.attackSequence;
        const auto damageSequence=player.state.damageSequence;
        player.state=common::PlayerState{};
        // Keep event counters monotonic when a slot is reused between snapshots.
        player.state.attackSequence=attackSequence;
        player.state.damageSequence=damageSequence;
        player.combat={}; player.attackInbox={}; player.lastInputSequence.reset();
        player.requestedVelocity={};
        triggers.reset(id);
    };

    sf::Clock snapshotClock;
    std::uint32_t snapshotSequence=0;
    sf::Clock frameClock;
    float accumulator = 0.f;
    float elapsedTime = 0.0f;
    common::Tick tick = 0;

    while (true) {
        const float dt = frameClock.restart().asSeconds();
        accumulator += dt;
        elapsedTime += dt;

        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        while (socket.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done) {
            if (!senderIp) {
                packet.clear();
                continue;
            }

            std::string type;
            packet >> type;

            if (type == common::MSG_JOIN) {
                std::string requestedName;
                if (!(packet >> requestedName)) continue;

                if (requestedName.size()>64) {
                    sf::Packet rejected; rejected << std::string(common::MSG_JOIN_ACK) << std::int32_t(-1);
                    sendPacket(socket,rejected,*senderIp,senderPort,"invalid name reply");
                    continue;
                }
                int assignedId = -1;
                for (int i=0;i<common::MAX_PLAYERS;++i) {
                    if (players[i].state.connected && players[i].ip==senderIp && players[i].port==senderPort) {
                        assignedId=i;
                        break;
                    }
                }
                const bool repeatedJoin=assignedId>=0;
                for (int i = 0; assignedId<0 && i < common::MAX_PLAYERS; ++i) {
                    if (!players[i].state.connected) {
                        assignedId = i;
                        logger.log_info("Assigned player id ", assignedId);
                        const auto spawn = findSpawn(players, collision, triggers);
                        if (!spawn) { assignedId = -1; break; }
                        players[i].state.pos = *spawn;
                        players[i].state.connected = true;
                        players[i].ip = *senderIp;
                        players[i].port = senderPort;
                        players[i].state.name =
                            requestedName.empty() ? ("Player" + std::to_string(i + 1)) : requestedName;
                        players[i].state.score = 0;
                        //players[i].state.pos = {150.f + 400.f * static_cast<float>(i), 300.f}; // nick todo: spawn point
                        break;
                    }
                }

                if (assignedId>=0) players[assignedId].lastHeard.restart();
                sf::Packet reply;
                reply << std::string(common::MSG_JOIN_ACK) << assignedId;

                // Only the requester receives its assignment, including full-server rejection.
                sendPacket(socket,reply,*senderIp,senderPort,"join_ack send");
                if (!repeatedJoin && assignedId>=0) {
                    sf::Packet joined;
                    joined << std::string("player_joined") << static_cast<common::PlayerId>(assignedId);
                    for (const auto& player:players) {
                        if (!player.state.connected || !player.ip) continue;
                        sendPacket(socket,joined,*player.ip,player.port,"player_joined send");
                    }
                }

                if (assignedId >= 0) {
                    logger.info() << "Join from " << senderIp->toString() << ":" << senderPort
                              << " -> player " << assignedId
                              << " name=" << players[assignedId].state.name << "\n";
                } else {
                    logger.info() << "Rejected join from " << senderIp->toString() << ":" << senderPort
                              << " (server full)\n";
                }
            } else if (type == "leave") {
                common::PlayerId id;
                if (!(packet >> id) || id>=players.size()) continue;
                auto& player=players[id];
                if (player.ip!=senderIp || player.port!=senderPort) continue;
                if (player.state.connected) disconnect(id);
                sf::Packet ack;
                ack << std::string("leave_ack") << id;
                sendPacket(socket,ack,*senderIp,senderPort,"leave ack");
            } else if (type == common::MSG_ATTACK) {
                common::PlayerId playerId;
                common::AttackRequest request;
                if (!common::readAttackRequest(packet,playerId,request) || playerId>=players.size()) continue;
                auto& player=players[playerId];
                if (!player.state.connected || player.ip!=senderIp || player.port!=senderPort) continue;
                player.lastHeard.restart();
                // ACK duplicates and expired/dead-player requests as well, so a lost
                // ACK never creates a second attack or an endless client retry.
                sf::Packet ack;
                ack << std::string(common::MSG_ATTACK_ACK) << request.sequence;
                sendPacket(socket,ack,*senderIp,senderPort,"attack ack");
                if (player.attackInbox.accept(request.sequence))
                    common::requestAttack(player.state,player.combat,request);
            } else if (type == common::MSG_STATE) {
                common::InputCommand cmd;
                common::PlayerId playerId;
                if (!readInputCmd(packet, playerId, cmd) || playerId >= players.size()) {
                    continue;
                }
                auto& player = players[playerId];
                if (!player.state.connected || player.ip != senderIp || player.port != senderPort) {
                    continue;
                }
                player.lastHeard.restart();
                // Ignore duplicate or out-of-order UDP inputs (including wraparound).
                if (player.lastInputSequence &&
                    (cmd.sequence - *player.lastInputSequence == 0 ||
                     cmd.sequence - *player.lastInputSequence >= 0x80000000u)) {
                    continue;
                }
                player.lastInputSequence = cmd.sequence;
                if (!player.state.alive) {
                    continue;
                }
                const float speed = cmd.sprint ? common::RUN_SPEED : common::WALK_SPEED;
                if (!std::isfinite(cmd.move.x) || !std::isfinite(cmd.move.y)) continue;
                const float length = std::hypot(cmd.move.x, cmd.move.y);
                if (length > 1.f) cmd.move /= length;
                player.requestedVelocity = cmd.move * speed;
                player.lastInputTime.restart();
            }

            packet.clear();
            senderIp.reset();
            senderPort = 0;
        }

        for (common::PlayerId id=0;id<players.size();++id) {
            auto& player=players[id];
            if (player.state.connected && player.ip && player.lastHeard.getElapsedTime()>=sf::seconds(5))
                disconnect(id);
        }

        // Fixed simulation clock
        while (accumulator >= common::TICK_DT) {
            accumulator -= common::TICK_DT;
            tick++;

            for (int i = 0; i < common::MAX_PLAYERS; i++) {
                auto& player = players[i];
                if (!player.state.connected) {
                    continue;
                }

                if (common::advanceDeath(player.state, player.combat)) {
                    if (const auto spawn = findSpawn(players, collision, triggers)) {
                        common::respawn(player.state, player.combat, *spawn);
                        player.requestedVelocity = {};
                        triggers.reset(i);
                    }
                    continue;
                }
                if (!player.state.alive) {
                    player.requestedVelocity = {};
                    triggers.reset(i);
                    continue;
                }
                std::vector<sf::Vector2f> blockers;
                for (int j = 0; j < common::MAX_PLAYERS; ++j) {
                    if (j != i && players[j].state.connected && players[j].state.alive)
                        blockers.push_back(players[j].state.pos);
                }
                const auto oldPosition = player.state.pos;
                if (player.ip) {
                    const auto velocity = player.lastInputTime.getElapsedTime().asSeconds() < 0.25f
                        ? player.requestedVelocity : sf::Vector2f{};
                    player.state.pos = collision.move(oldPosition, velocity * common::TICK_DT, common::CollisionWorld::PlayerRadius, blockers);
                    player.state.vel = (player.state.pos - oldPosition) / common::TICK_DT;
                }

                // Bot moves in a lissajous-like path
                if (i == botId) {
                    common::PlayerState& s = players[botId].state;
                    const float t = elapsedTime;
                    const sf::Vector2f newPos{
                        std::sin(t * 1.2f) * 350.f + 600.f,
                        std::cos(t * 0.7f) * 180.f + 600.f
                    };

                    auto movement = newPos - oldPosition;
                    const float distance = movement.length();
                    const float maxStep = common::RUN_SPEED * common::TICK_DT;
                    if (distance > maxStep) movement *= maxStep / distance;
                    s.pos = collision.move(oldPosition, movement, common::CollisionWorld::PlayerRadius, blockers);
                    s.vel = (s.pos - oldPosition) / common::TICK_DT;
                }
                if (player.state.vel.lengthSquared()>0.0001f)
                    player.combat.facing=player.state.vel.normalized();
                triggers.update(i, player.state);
            }
            std::vector<common::PlayerState*> targets;
            for (auto& player : players) targets.push_back(&player.state);
            for (auto& player : players) {
                if (player.state.connected)
                    common::updateAttack(player.state, player.combat, targets, collision);
            }
        }

        if (snapshotClock.getElapsedTime()<sf::seconds(1.f/30.f)) {
            sf::sleep(sf::milliseconds(1));
            continue;
        }
        snapshotClock.restart();
        std::vector<common::PlayerState> publicStates(common::MAX_PLAYERS);
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            publicStates[i] = players[i].state;
            const auto& combat=players[i].combat;
            publicStates[i].combatDebug={combat.attack,combat.age,combat.attackDirection,combat.hit,combat.hitTarget};
        }

        auto packets=common::worldPackets(publicStates,++snapshotSequence);
        for (const auto& player : players) {
            if (!player.state.connected || !player.ip) continue;
            for (auto& part:packets) sendPacket(socket,part,*player.ip,player.port,"world part send");
        }
    }

    return 0;
}