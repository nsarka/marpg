#include "client/client_connection.hpp"
#include "server/connection_manager.hpp"
#include <atomic>
#include <sstream>
#include <thread>

void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct RunningServer {
    GameSimulation& game;
    ConnectionManager& connection;
    std::atomic<bool> stop{false};
    std::atomic<int> killAlice{0};
    std::thread worker;
    RunningServer(GameSimulation& g, ConnectionManager& c)
        : game(g), connection(c), worker([this] {
              const volatile std::sig_atomic_t signal = 0;
              while (!stop.load()) {
                  connection.pump(signal);
                  // No simulation tick: these positions deliberately isolate chat from
                  // distance, collision, and out-of-bounds gameplay.
                  for (std::size_t i = 0; i < game.players.size(); ++i)
                      if (game.players[i].state.connected)
                          game.players[i].state.pos = {float(i) * 100000, 0};
                  if (killAlice.load() == 1) {
                      game.players[0].state.alive = false;
                      game.players[0].state.health = 0;
                      ++game.players[0].state.deaths;
                      killAlice = 2;
                  }
                  connection.broadcast();
                  sf::sleep(sf::milliseconds(20));
              }
          }) {}
    ~RunningServer() {
        stop = true;
        if (worker.joinable())
            worker.join();
    }
};
int main() {
    sf::UdpSocket reservation;
    check(reservation.bind(0, sf::IpAddress::LocalHost) == sf::Socket::Status::Done,
          "Port allocation failed");
    const auto port = reservation.getLocalPort();
    reservation.unbind();
    common::ServerSettings settings;
    settings.ip = "127.0.0.1";
    settings.port = port;
    settings.map = "arena";
    settings.slots = 3;
    settings.teams = 2;
    settings.bots = 0;
    settings.botAI = false;
    std::ostringstream logs;
    common::Logger serverLogger(logs), clientLogger;
    GameSimulation game(settings, serverLogger);
    ConnectionManager manager(game, serverLogger);
    RunningServer running(game, manager);
    sf::UdpSocket alice;
    check(alice.bind(0) == sf::Socket::Status::Done, "Alice socket bind failed");
    alice.setBlocking(false);
    auto sendAlice = [&](std::uint32_t sequence, const std::string& text) {
        sf::Packet p;
        p << std::string(common::MSG_CHAT_SEND) << common::PlayerId(0) << sequence << text;
        check(alice.send(p, sf::IpAddress::LocalHost, port) == sf::Socket::Status::Done,
              "Alice chat send failed");
    };
    sf::Packet join;
    join << std::string(common::MSG_JOIN) << std::string("Alice") << std::uint32_t(1)
         << std::string(common::BuildCommit);
    check(alice.send(join, sf::IpAddress::LocalHost, port) == sf::Socket::Status::Done,
          "Alice join send failed");
    sf::Clock wait;
    bool joined = false;
    while (wait.getElapsedTime() < sf::seconds(2) && !joined) {
        sf::Packet p;
        std::optional<sf::IpAddress> ip;
        unsigned short from;
        if (alice.receive(p, ip, from) == sf::Socket::Status::Done) {
            std::string type;
            common::PlayerId id;
            if ((p >> type >> id) && type == common::MSG_JOIN_ACK) {
                check(id == 0, "Alice slot mismatch");
                joined = true;
            }
        }
        sf::sleep(sf::milliseconds(5));
    }
    check(joined, "Alice join timeout");
    ClientConnection bob(clientLogger), cara(clientLogger);
    check(bob.connectToServer("127.0.0.1", "Bob", port, 2) == 1, "Bob join failed");
    check(cara.connectToServer("127.0.0.1", "Cara", port, 1) == 2, "Cara join failed");
    std::vector<common::PlayerState> states(common::MAX_PLAYERS);
    std::vector<common::PlayerId> joins;
    auto collect = [&](ClientConnection& client) {
        client.pumpNetwork(states, joins);
        return client.takeChatMessages();
    };
    sf::sleep(sf::milliseconds(100));
    const auto joinedMessages = collect(bob);
    check(std::count_if(joinedMessages.begin(), joinedMessages.end(),
                        [](const auto& message) { return message.text == "Cara joined the game."; }) == 1,
          "Join notice missing or duplicated");
    collect(cara);
    sendAlice(1, "Hello everyone");
    sendAlice(1, "Hello everyone");
    sf::sleep(sf::milliseconds(600)); // Force server retries before receiving/ACKing.
    auto b = collect(bob), c = collect(cara);
    check(b.size() == 1 && c.size() == 1 && b[0].name == "Alice" && !b[0].dead &&
              b[0].text == "Hello everyone",
          "Global chat delivery or deduplication failed");
    check(states[1].pos.x - states[0].pos.x > 90000, "Distance fixture was not applied");
    sendAlice(1, "Hello everyone");
    sf::sleep(sf::milliseconds(80));
    check(collect(bob).empty(), "Duplicate request broadcast again");
    check(bob.sendChat("Hello Alice"), "Real client could not enqueue chat");
    collect(bob);
    sf::sleep(sf::milliseconds(80));
    b = collect(bob);
    c = collect(cara);
    check(b.size() == 1 && c.size() == 1 && b[0].name == "Bob", "Client send/self echo failed");
    running.killAlice = 1;
    wait.restart();
    while (running.killAlice.load() != 2 && wait.getElapsedTime() < sf::seconds(2))
        sf::sleep(sf::milliseconds(5));
    check(running.killAlice.load() == 2, "Death setup timeout");
    sendAlice(2, "Still here");
    sf::sleep(sf::milliseconds(80));
    b = collect(bob);
    c = collect(cara);
    check(b.size() == 1 && c.size() == 1 && common::chatLine(b[0]) == "*DEAD* Alice: Still here",
          "Dead global message missing");
    sf::UdpSocket intruder;
    check(intruder.bind(0) == sf::Socket::Status::Done, "Intruder socket bind failed");
    sf::Packet forged;
    forged << std::string(common::MSG_CHAT_SEND) << common::PlayerId(0) << std::uint32_t(999)
           << std::string("forged");
    check(intruder.send(forged, sf::IpAddress::LocalHost, port) == sf::Socket::Status::Done,
          "Forge send failed");
    sf::sleep(sf::milliseconds(100));
    check(collect(bob).empty() && collect(cara).empty(), "Unowned sender slot accepted");
    bob.leaveServer();
    sf::sleep(sf::milliseconds(100));
    const auto departed = collect(cara);
    check(departed.size() == 1 && departed[0].text == "Bob left the game.",
          "Leave notice missing or duplicated");
    cara.leaveServer();
    running.stop = true;
    running.worker.join();
    const auto output = logs.str();
    for (const auto& expected : {"[Chat] Alice: Hello everyone", "[Chat] *DEAD* Alice: Still here",
                                 "[Chat] Cara joined the game.", "[Chat] Bob left the game."}) {
        const auto found = output.find(expected);
        check(found != std::string::npos && output.find(expected, found + 1) == std::string::npos,
              "Chat console logging missing or duplicated");
    }
    std::cout
        << "Chat broadcast, self echo, dead status, retries, deduplication, and sender validation passed\n";
}
