#include "client/chat_ui.hpp"
#include "client/client_connection.hpp"
#include "server/server_console.hpp"
#include <atomic>
#include <mutex>
#include <sstream>
#include <thread>
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Server {
    std::ostringstream output;
    common::Logger logger{output};
    std::unique_ptr<GameSimulation> game;
    std::unique_ptr<ConnectionManager> connections;
    std::unique_ptr<ServerConsole> console;
    std::mutex mutex;
    std::vector<std::string> commands;
    std::atomic<bool> stop{false};
    std::thread worker;
    explicit Server(unsigned short port) {
        common::ServerSettings settings;
        settings.map = "arena";
        settings.port = port;
        settings.ip = "127.0.0.1";
        settings.bots = 0;
        settings.botAI = false;
        settings.slots = 4;
        settings.teams = 2;
        game = std::make_unique<GameSimulation>(settings, logger);
        connections = std::make_unique<ConnectionManager>(*game, logger);
        console = std::make_unique<ServerConsole>(game, *connections, logger);
        worker = std::thread([this] {
            const volatile std::sig_atomic_t signal = 0;
            while (!stop) {
                connections->pump(signal);
                std::vector<std::string> lines;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    lines.swap(commands);
                }
                for (const auto& line : lines)
                    console->execute(line);
                console->update();
                connections->broadcast();
                sf::sleep(sf::milliseconds(10));
            }
        });
    }
    void command(const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);
        commands.push_back(value);
    }
    ~Server() {
        stop = true;
        worker.join();
        console.reset();
    }
};
int main() {
    check(common::slashCommand("/name Alice Smith")->argument == "Alice Smith",
          "Command arguments lost spaces");
    check(!common::commandNumber("-1") && !common::commandNumber("2garbage") &&
              !common::commandNumber("99999999999"),
          "Invalid count accepted");
    ChatUI local;
    check(local.localCommand("/help") && local.localCommand("/clear") && !local.localCommand("/team 2"),
          "Local command dispatch failed");
    sf::UdpSocket reserve;
    check(reserve.bind(0) == sf::Socket::Status::Done, "Port allocation failed");
    auto port = reserve.getLocalPort();
    reserve.unbind();
    Server server(port);
    common::Logger logger;
    ClientConnection alice(logger), bob(logger);
    check(alice.connectToServer("127.0.0.1", "Alice", port, 1) == 0, "Alice join failed");
    check(bob.connectToServer("127.0.0.1", "Bob", port, 2) == 1, "Bob join failed");
    std::vector<common::PlayerState> a(common::MAX_PLAYERS), b(common::MAX_PLAYERS);
    std::vector<common::PlayerId> joined;
    std::vector<common::ChatMessage> aMessages, bMessages;
    auto pump = [&] {
        alice.pumpNetwork(a, joined);
        bob.pumpNetwork(b, joined);
        for (auto& m : alice.takeChatMessages())
            aMessages.push_back(m);
        for (auto& m : bob.takeChatMessages())
            bMessages.push_back(m);
    };
    auto wait = [&](auto ready, const char* error) {
        sf::Clock clock;
        while (clock.getElapsedTime() < sf::seconds(8)) {
            pump();
            if (ready())
                return;
            sf::sleep(sf::milliseconds(10));
        }
        throw std::runtime_error(error);
    };
    auto has = [&](const auto& messages, const std::string& text, common::ChatKind kind) {
        return std::any_of(messages.begin(), messages.end(),
                           [&](const auto& m) { return m.text == text && m.kind == kind; });
    };
    server.command("Hello everyone");
    server.command("/announce Round starts soon");
    wait(
        [&] {
            return has(aMessages, "Hello everyone", common::ChatKind::Server) &&
                   has(bMessages, "Round starts soon", common::ChatKind::Announcement);
        },
        "Console messages not broadcast");
    check(alice.sendChat("/name Alice Smith"), "Name command queue failed");
    wait([&] { return a[0].name == "Alice Smith" && b[0].name == "Alice Smith"; }, "Name did not propagate");
    check(alice.sendChat("/team 2"), "Team command queue failed");
    wait([&] { return a[0].team == 1 && b[0].team == 1; }, "Team did not change");
    check(alice.sendChat("/map demo"), "Command queue failed");
    wait([&] { return has(aMessages, "Unknown client command. Use /help.", common::ChatKind::System); },
         "Client gained server command access");
    server.command("/bots 2");
    wait(
        [&] {
            return alice.serverSettings().bots == 2 && alice.isBot(2) && alice.isBot(3) && a[2].connected &&
                   a[3].connected;
        },
        "Bots not added to free slots");
    check(a[0].name == "Alice Smith" && a[1].name == "Bob" && !alice.isBot(0) && !alice.isBot(1),
          "Bots displaced humans");
    server.command("/bots 3");
    server.command("/map ../invalid");
    server.command("/map does-not-exist");
    server.command("Still running");
    wait([&] { return has(aMessages, "Still running", common::ChatKind::Server); },
         "Invalid commands stopped server");
    check(alice.serverSettings().bots == 2 && alice.serverSettings().map == "arena",
          "Rejected command changed settings");
    server.command("/ai on");
    wait([&] { return alice.serverSettings().botAI; }, "Background navigation did not enable bot AI");
    server.command("/bots 0");
    server.command("/ai on");
    wait([&] { return alice.serverSettings().bots == 0 && alice.serverSettings().botAI; },
         "AI setting not synchronized");
    server.command("/ai off");
    wait([&] { return !alice.serverSettings().botAI; }, "AI off not synchronized");
    auto epoch = alice.worldVersion();
    server.command("/restart");
    wait([&] { return alice.worldReloadRequested() && bob.worldReloadRequested(); }, "Restart not delivered");
    check(alice.worldVersion() != epoch && a[0].name == "Alice Smith", "Restart lost player identity");
    alice.beginWorldLoad();
    bob.beginWorldLoad();
    alice.markWorldLoaded(alice.worldVersion());
    bob.markWorldLoaded(bob.worldVersion());
    server.command("/map farms");
    wait(
        [&] {
            return alice.worldReloadRequested() && bob.worldReloadRequested() &&
                   alice.serverSettings().map == "farms";
        },
        "Map change not synchronized");
    alice.beginWorldLoad();
    bob.beginWorldLoad();
    alice.markWorldLoaded(alice.worldVersion());
    bob.markWorldLoaded(bob.worldVersion());
    wait([&] { return a[0].connected && a[0].name == "Alice Smith" && a[0].team == 1 && a[1].connected; },
         "Map change lost roster");
    check(alice.sendChat("After map change"), "Chat after map change queue failed");
    wait([&] { return has(bMessages, "After map change", common::ChatKind::Player); },
         "Chat stopped after map change");
    std::cout << "Console chat/announcements, client commands, bot slots, AI, restart, map reload, and "
                 "access checks passed\n";
}
