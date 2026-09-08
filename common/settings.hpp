#pragma once
#include "common.hpp"
#include <stdexcept>

namespace common {
struct ServerSettings {
    std::string ip="0.0.0.0";
    std::uint16_t port=54000;
    std::uint32_t slots=20, teams=2, bots=10;
    int triggerDamage=2, boundsDamage=2, jabDamage=20, hookDamage=35;
    double triggerBpm=120, boundsBpm=120;
    bool friendlyFire=false;
    bool honorTeamRequests=true;
    void validate() const;
};
struct ClientSettings {std::string name="Rick",ip="127.0.0.1";std::uint16_t port=54000;std::uint32_t team=0;bool showOtherDamageNumbers=true;};
ServerSettings loadServerSettings(const std::string& path);
ClientSettings loadClientSettings(const std::string& path);
inline ServerSettings activeSettings{};
inline constexpr std::uint32_t ProtocolVersion=3;
void applySettings(const ServerSettings& settings);
void writeSettings(sf::Packet& packet,const ServerSettings& settings);
bool readSettings(sf::Packet& packet,ServerSettings& settings);
sf::Color teamColor(std::uint32_t team,std::uint32_t count);
inline Tick damageInterval(double bpm) {return static_cast<Tick>(std::llround(60.0*TICK_RATE/bpm));}
}
