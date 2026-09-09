#pragma once
#include "common.hpp"
#include "keybindings.hpp"
#include <stdexcept>

namespace common {
struct SpellSettings {
    int damageMin=5,damageMax=30;
    double radius=120,range=500,windup=.375,cooldown=1.625,duration=0,interval=1.0/3.0;
};
struct ServerSettings {
    int jabDamageMin=20, hookDamageMin=35;
    double jabConeDegrees=90, hookConeDegrees=90;
    double damageStunSeconds=.5;
    double respawnSeconds=2.5;
    double jabRange=70, hookRange=80;
    double jabWindup=.25, hookWindup=.375;
    SpellSettings spell;
    SpellSettings lightning{1,4,45,500,.25,1,3,1.0/3.0};
    std::string name="Rick's Funhaus";
    std::string ip="0.0.0.0";
    std::uint16_t port=54000;
    std::uint32_t slots=20, teams=2, bots=10;
    int triggerDamage=2, boundsDamage=2, jabDamage=20, hookDamage=35;
    double triggerInterval=.5, boundsInterval=.5;
    bool botAI=true;
    bool friendlyFire=false;
    bool honorTeamRequests=true;
    void validate() const;
};
struct ClientSettings {double mouseIdleSeconds=3;KeyBindings bindings;std::string name="Rick",ip="147.182.213.239";std::uint16_t port=54000;std::uint32_t team=0;bool showOtherDamageNumbers=true;};
ServerSettings loadServerSettings(const std::string& path);
ClientSettings loadClientSettings(const std::string& path);
inline ServerSettings activeSettings{};
inline constexpr std::uint32_t ProtocolVersion=4;
void applySettings(const ServerSettings& settings);
void writeSettings(sf::Packet& packet,const ServerSettings& settings);
bool readSettings(sf::Packet& packet,ServerSettings& settings);
sf::Color teamColor(std::uint32_t team,std::uint32_t count);
inline Tick damageInterval(double seconds) {return std::max(1u,static_cast<Tick>(std::llround(seconds*TICK_RATE)));}
}
