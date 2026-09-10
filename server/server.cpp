#include "connection_manager.hpp"
#include <csignal>
#include <iostream>
namespace {
volatile std::sig_atomic_t stopRequested = 0;
void requestStop(int) { stopRequested = 1; }
} // namespace
int main(int argc, char **) {
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
        GameSimulation simulation(common::loadServerSettings("../server.toml"), logger);
        ConnectionManager connections(simulation, logger);
        sf::Clock frameClock, snapshotClock;
        float accumulator = 0;
        while (!stopRequested) {
            accumulator += frameClock.restart().asSeconds();
            connections.pump(stopRequested);
            while (!stopRequested && accumulator >= common::TICK_DT) {
                accumulator -= common::TICK_DT;
                simulation.step();
            }
            if (snapshotClock.getElapsedTime() >= sf::seconds(1.f / 30.f)) {
                snapshotClock.restart();
                connections.broadcast();
            } else
                sf::sleep(sf::milliseconds(1));
        }
        connections.shutdown();
    } catch (const std::exception &error) {
        logger.log_error(error.what());
        return 1;
    }
    return 0;
}
