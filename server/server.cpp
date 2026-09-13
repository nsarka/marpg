#include "connection_manager.hpp"
#include "console_input.hpp"
#include "server_console.hpp"
#include <csignal>
#include <iomanip>
#include <iostream>
namespace {
volatile std::sig_atomic_t stopRequested = 0;
void requestStop(int) {
    stopRequested = 1;
}
} // namespace
int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "Server does not accept command-line arguments. Edit server.toml instead.\n";
        return 1;
    }
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
#ifdef SIGHUP
    std::signal(SIGHUP, requestStop);
#endif
    common::Logger logger;
    logger.info() << "Server started";
    try {
        std::string lastStage;
        double lastTime = -1;
        common::LoadProgress progress(
            [&](const common::LoadStatus& s) {
                if (!s.finished && s.stage == lastStage && s.totalSeconds - lastTime < 1)
                    return;
                {
                    auto line = logger.info();
                    line << s.stage << (s.finished ? " complete" : "");
                    if (s.total)
                        line << " " << s.completed << "/" << s.total;
                    line << std::fixed << std::setprecision(2) << " (stage " << s.stageSeconds << "s, total "
                         << s.totalSeconds << "s)";
                }
                logger.flush();
                lastStage = s.stage;
                lastTime = s.totalSeconds;
            },
            [] { return stopRequested != 0; });
        auto simulation =
            std::make_unique<GameSimulation>(common::loadServerSettings("../server.toml"), logger, &progress);
        progress.report("Starting network");
        ConnectionManager connections(*simulation, logger);
        ConsoleInput consoleInput;
        ServerConsole console(simulation, connections, logger);
        progress.finish();
        if (!stopRequested) {
            logger.log_info("Loading complete. Game started.");
            logger.flush();
        }
        sf::Clock frameClock, snapshotClock;
        float accumulator = 0;
        while (!stopRequested) {
            accumulator += frameClock.restart().asSeconds();
            connections.pump(stopRequested);
            for (const auto& line : consoleInput.poll())
                console.execute(line);
            if (console.update()) {
                accumulator = 0;
                frameClock.restart();
            }
            while (!stopRequested && accumulator >= common::TICK_DT) {
                accumulator -= common::TICK_DT;
                simulation->step();
            }
            if (snapshotClock.getElapsedTime() >= sf::seconds(1.f / 30.f)) {
                snapshotClock.restart();
                connections.broadcast();
            } else
                sf::sleep(sf::milliseconds(1));
        }
        connections.shutdown();
    } catch (const common::LoadingCancelled&) {
        logger.log_info("Server loading cancelled");
    } catch (const std::exception& error) {
        logger.log_error(error.what());
        return 1;
    }
    return 0;
}
