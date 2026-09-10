#include "client/client_connection.hpp"
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 3)
        return 1;
    common::Logger logger;
    ClientConnection connection(logger);
    auto id = connection.connectToServer("127.0.0.1", "Shutdown probe",
                                         static_cast<unsigned short>(std::stoi(argv[1])));
    if (id >= common::MAX_PLAYERS)
        return 2;
    std::cout << "READY\n" << std::flush;
    std::vector<common::PlayerState> players(common::MAX_PLAYERS);
    std::vector<common::PlayerId> joined;
    sf::Clock deadline;
    common::InputCommand input;
    while (deadline.getElapsedTime() < sf::seconds(14)) {
        connection.pumpNetwork(players, joined);
        if (connection.shuttingDown()) {
            const std::string expected = std::string(argv[2]) == "notice"
                                             ? "Server is shutting down."
                                             : "Connection to the server was lost.";
            if (connection.shutdownReason() != expected)
                return 3;
            std::cout << "PASS: client received " << connection.shutdownReason() << '\n';
            return 0;
        }
        ++input.sequence;
        connection.sendInput(id, input);
        sf::sleep(sf::milliseconds(16));
    }
    return 4;
}
