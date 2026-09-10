#include "client/client_connection.hpp"
#include "client/loading_connection.hpp"
#include "common/settings.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3)return 1;
    common::Logger logger;ClientConnection connection(logger);
    const auto id=connection.connectToServer("127.0.0.1","Config probe",static_cast<unsigned short>(std::stoi(argv[1])));
    if(id>=common::MAX_PLAYERS)return 2;
    if(argc==3) {
        LoadingConnection loading(connection);
        std::this_thread::sleep_for(std::chrono::seconds(7));
        std::vector<common::PlayerState> states;std::vector<common::PlayerId> joined;
        loading.finish(states,joined);
        if(connection.shuttingDown() || !connection.hasWorldSnapshot() || states.size()<=id || !states[id].connected)return 5;
        auto playerId=id;common::InputCommand input;input.sequence=1;connection.sendInput(playerId,input);
        connection.pumpNetwork(states,joined);
        if(connection.shuttingDown())return 6;
        std::cout<<"PASS: loading client stayed connected beyond idle timeout and handed off to gameplay\n";
        return 0;
    }
    const auto& s=connection.serverSettings();
    if(s.port!=std::stoi(argv[1]) || s.teams!=3 || s.slots!=6 || s.bots!=0 || s.lightDamage!=27 || s.heavyDamage!=41 || s.triggerDamage!=7 || s.boundsDamage!=9 || s.triggerInterval!=1.0 || s.boundsInterval!=.25)return 3;
    if(common::attackDescription(common::AttackKind::Heavy,s).damage!=41)return 4;
    connection.leaveServer();
    std::cout<<"PASS: real client handshake applied server TOML settings on custom port\n";
}
