#include "common/common.hpp"
#include "common/world_transport.hpp"
#include "common/team_spawns.hpp"
#include "common/bot_ai.hpp"
#include "common/kill_history.hpp"
#include "common/collision_world.hpp"
#include "common/trigger_system.hpp"
#include "common/combat_system.hpp"
#include "common/logger.hpp"

#include <SFML/Network.hpp>

#include <cstdlib>
#include <csignal>
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
    sf::Clock pingClock;
    std::uint32_t pingSequence=0;
    bool pingPending=false;
};

namespace {
volatile std::sig_atomic_t stopRequested=0;
void requestStop(int) {stopRequested=1;}

bool sendPacket(sf::UdpSocket& socket, sf::Packet& packet, const sf::IpAddress& ip, unsigned short port, const char* context) {
    const sf::Socket::Status status = socket.send(packet, ip, port);
    if (status != sf::Socket::Status::Done) {
        std::cerr << context << " failed with socket status "
                  << static_cast<int>(status) << '\n';
        return false;
    }
    return true;
}

std::vector<common::PlayerState> states(const std::vector<ServerPlayer>& players) {
    std::vector<common::PlayerState> result;for(const auto& p:players)result.push_back(p.state);return result;
}

} // namespace

int main(int argc,char**) {
    if(argc!=1) {
        std::cerr<<"Server does not accept command-line arguments. Edit server.toml instead.\n";
        return 1;
    }
    std::signal(SIGINT,requestStop);
    std::signal(SIGTERM,requestStop);
#ifdef SIGHUP
    std::signal(SIGHUP,requestStop);
#endif
    common::Logger logger;

    logger.info() << "Server started";

    common::ServerSettings settings;
    try {
        settings=common::loadServerSettings("../server.toml");
        common::applySettings(settings);
    } catch(const std::exception& error){logger.log_error(error.what());return 1;}

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

    common::TeamSpawns spawns;
    try {spawns.load(common::LEVEL_PATH,settings.teams,collision,triggers);}
    catch(const std::exception& error){logger.log_error(error.what());return 1;}
    common::Navigation navigation(common::LEVEL_PATH,collision,triggers);
    common::BotAI botAI(navigation,collision);
    const auto bindIp=sf::IpAddress::resolve(settings.ip);
    if(!bindIp){logger.log_error("Invalid bind IP: ",settings.ip);return 1;}
    sf::UdpSocket socket;
    if (socket.bind(settings.port,*bindIp) != sf::Socket::Status::Done) {
        logger.log_error("Failed to bind server socket on port ", settings.port);
        return 1;
    }
    socket.setBlocking(false);

    std::vector<ServerPlayer> players(common::MAX_PLAYERS);
    for(unsigned i=0;i<settings.bots;++i) {
        auto team=common::smallestTeam(states(players),settings.teams);
        auto spawn=spawns.choose(team,states(players),collision,triggers);
        if(!spawn){logger.log_error("No safe bot spawn");return 1;}
        players[i].state.connected=true;players[i].state.team=team;
        players[i].state.name="Bot "+std::to_string(i+1);players[i].state.pos=*spawn;
    }

    logger.log_info("Server listening on port ", settings.port);
    std::cout.flush();

    const auto disconnect=[&](common::PlayerId id) {
        auto& player=players[id];
        logger.log_info("Player disconnected: ",id," (",player.state.name,")");
        const auto spellSequence=player.state.spellSequence;
        const auto attackSequence=player.state.attackSequence;
        const auto damageSequence=player.state.damageSequence;
        player.state=common::PlayerState{};
        // Keep event counters monotonic when a slot is reused between snapshots.
        player.state.attackSequence=attackSequence;
        player.state.spellSequence=spellSequence;
        player.state.damageSequence=damageSequence;
        player.combat={}; player.attackInbox={}; player.lastInputSequence.reset();
        player.requestedVelocity={};
        triggers.reset(id);
    };

    common::KillHistory killHistory;
    sf::Clock snapshotClock;
    std::uint32_t snapshotSequence=0;
    sf::Clock frameClock;
    float accumulator = 0.f;
    common::Tick tick = 0;

    while (!stopRequested) {
        const float dt = frameClock.restart().asSeconds();
        accumulator += dt;

        sf::Packet packet;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort = 0;

        while (!stopRequested && socket.receive(packet, senderIp, senderPort) == sf::Socket::Status::Done) {
            if (!senderIp) {
                packet.clear();
                continue;
            }

            std::string type;
            packet >> type;

            if (type == common::MSG_JOIN) {
                std::string requestedName;
                if (!(packet >> requestedName)) continue;
                std::uint32_t requestedTeam=0;
                if(!packet.endOfPacket() && !(packet >> requestedTeam))continue;

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
                for (int i = 0; assignedId<0 && i < static_cast<int>(settings.slots); ++i) {
                    if (!players[i].state.connected) {
                        assignedId = i;
                        logger.log_info("Assigned player id ", assignedId);
                        const auto team=common::chooseTeam(states(players),settings,requestedTeam);
                        const auto spawn = spawns.choose(team,states(players),collision,triggers);
                        if (!spawn) { assignedId = -1; break; }
                        players[i].state.team=team;
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
                reply << std::string(common::MSG_JOIN_ACK) << assignedId << common::ProtocolVersion;
                common::writeSettings(reply,settings);

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
            } else if (type == "pong") {
                common::PlayerId id;std::uint32_t sequence;
                if(!(packet>>id>>sequence) || id>=players.size())continue;
                auto& player=players[id];
                if(!player.state.connected || player.ip!=senderIp || player.port!=senderPort ||
                   !player.pingPending || sequence!=player.pingSequence)continue;
                player.state.pingMs=player.pingClock.getElapsedTime().asMilliseconds();
                player.pingPending=false;
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
                if((request.kind==common::AttackKind::Uppercut || request.kind==common::AttackKind::Lightning) &&
                   !common::spellTargetValid(player.state,request.kind,request.aim,collision))continue;
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
                if(cmd.hasCursor) {
                    const auto previousFacing=player.combat.facing;
                    common::updateAim(player.state,player.combat,cmd.cursor,collision);
                    const auto kind=player.combat.attack;
                    const bool spellWindup=(kind==common::AttackKind::Uppercut || kind==common::AttackKind::Lightning) &&
                        player.combat.age<common::attackDescription(kind).startupTicks;
                    if(cmd.movementFacing && !spellWindup)
                        player.combat.facing=cmd.move.length()>.001f?cmd.move.normalized():previousFacing;
                }
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
        while (!stopRequested && accumulator >= common::TICK_DT) {
            accumulator -= common::TICK_DT;
            tick++;

            for (int i = 0; i < common::MAX_PLAYERS; i++) {
                auto& player = players[i];
                if (!player.state.connected) {
                    continue;
                }

                if (common::advanceDeath(player.state, player.combat)) {
                    if (const auto spawn = spawns.choose(player.state.team,states(players),collision,triggers)) {
                        common::respawn(player.state, player.combat, *spawn);
                        player.requestedVelocity = {};
                        triggers.reset(i);
                        botAI.reset(i);
                    }
                    continue;
                }
                if (!player.state.alive) {
                    player.requestedVelocity = {};
                    triggers.reset(i);
                    botAI.reset(i);
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

                if (i<static_cast<int>(settings.bots) && settings.botAI) {
                    std::vector<common::PlayerState*> opponents;
                    for(auto& other:players)opponents.push_back(&other.state);
                    botAI.update(i,player.state,player.combat,opponents);
                }

                triggers.update(i, player.state);
            }
            std::vector<common::PlayerState*> targets;
            std::vector<common::CombatState*> targetCombats;
            for (auto& player : players) {
                targets.push_back(&player.state);
                targetCombats.push_back(&player.combat);
            }
            for (auto& player : players) {
                if (player.state.connected)
                    common::updateAttack(player.state, player.combat, targets, collision, targetCombats);
            }
            killHistory.observe(targets,triggers);
        }

        if (snapshotClock.getElapsedTime()<sf::seconds(1.f/30.f)) {
            sf::sleep(sf::milliseconds(1));
            continue;
        }
        snapshotClock.restart();
        std::vector<common::PlayerState> publicStates(common::MAX_PLAYERS);
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& player=players[i];
            if(player.state.connected && player.ip && player.pingClock.getElapsedTime()>=sf::seconds(1)) {
                sf::Packet ping;ping<<std::string("ping")<<static_cast<common::PlayerId>(i)<<++player.pingSequence;
                sendPacket(socket,ping,*player.ip,player.port,"ping");
                if(player.pingPending)player.state.pingMs=-1;
                player.pingPending=true;player.pingClock.restart();
            }
            publicStates[i] = players[i].state;
            if(!player.ip)publicStates[i].pingMs=0;
            const auto& combat=players[i].combat;
            publicStates[i].facing=combat.facing;
            publicStates[i].combatDebug={combat.attack,combat.age,combat.attack==common::AttackKind::None?combat.facing:combat.attackDirection,combat.hit,combat.hitTarget};
        }

        auto packets=common::worldPackets(publicStates,++snapshotSequence,killHistory.events());
        for (const auto& player : players) {
            if (!player.state.connected || !player.ip) continue;
            for (auto& part:packets) sendPacket(socket,part,*player.ip,player.port,"world part send");
        }
    }

    logger.log_info("Server is shutting down. Notifying clients...");
    std::vector<bool> pending(players.size());
    for(std::size_t i=0;i<players.size();++i)pending[i]=players[i].state.connected && players[i].ip.has_value();
    sf::Clock shutdownClock,retry;bool first=true;
    while(shutdownClock.getElapsedTime()<sf::milliseconds(800) && std::any_of(pending.begin(),pending.end(),[](bool value){return value;})) {
        if(first || retry.getElapsedTime()>=sf::milliseconds(100)) {
            sf::Packet notice;notice << std::string("server_shutdown");
            for(std::size_t i=0;i<players.size();++i)if(pending[i])sendPacket(socket,notice,*players[i].ip,players[i].port,"shutdown notice");
            first=false;retry.restart();
        }
        sf::Packet packet;std::optional<sf::IpAddress> address;unsigned short port=0;
        while(socket.receive(packet,address,port)==sf::Socket::Status::Done) {
            std::string kind;common::PlayerId id;
            if((packet >> kind >> id) && kind=="shutdown_ack" && id<players.size() && players[id].ip==address && players[id].port==port)pending[id]=false;
            if(shutdownClock.getElapsedTime()>=sf::milliseconds(800))break;
        }
        sf::sleep(sf::milliseconds(5));
    }
    return 0;
}