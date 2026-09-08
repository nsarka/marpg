#include "client/client_connection.hpp"
#include "common/settings.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    common::Logger logger;ClientConnection connection(logger);
    const auto id=connection.connectToServer("127.0.0.1","Config probe",static_cast<unsigned short>(std::stoi(argv[1])));
    if(id>=common::MAX_PLAYERS)return 2;
    const auto& s=common::activeSettings;
    if(s.port!=std::stoi(argv[1]) || s.teams!=3 || s.slots!=6 || s.bots!=0 || s.jabDamage!=27 || s.hookDamage!=41 || s.triggerDamage!=7 || s.boundsDamage!=9 || s.triggerBpm!=60 || s.boundsBpm!=240)return 3;
    if(common::attackDescription(common::AttackKind::Hook).damage!=41)return 4;
    connection.leaveServer();
    std::cout<<"PASS: real client handshake applied server TOML settings on custom port\n";
}
