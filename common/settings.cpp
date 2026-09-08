#include "settings.hpp"
#include "third_party/tomlplusplus/toml.hpp"
#include <algorithm>
#include <set>

namespace common {
namespace {
template<typename T> T get(const toml::table& table,const std::string& section,const std::string& key,T fallback) {
    auto node=table[section][key];
    if (!node) return fallback;
    auto value=node.value<T>();
    if (!value) throw std::runtime_error("Invalid type for "+section+"."+key);
    return *value;
}
void keys(const toml::table& root,const std::string& section,std::initializer_list<std::string> allowed) {
    if (!root.contains(section)) return;
    auto* table=root[section].as_table();
    if (!table) throw std::runtime_error(section+" must be a table");
    for (const auto& [key,value]:*table)
        if (std::find(allowed.begin(),allowed.end(),key.str())==allowed.end())
            throw std::runtime_error("Unknown setting: "+section+"."+std::string(key.str()));
}
std::uint32_t integer(const toml::table& t,const char* section,const char* key,std::uint32_t fallback,std::uint32_t max) {
    auto value=get<std::int64_t>(t,section,key,fallback);
    if (value<0 || value>max) throw std::runtime_error(std::string("Out of range: ")+section+"."+key);
    return static_cast<std::uint32_t>(value);
}
}
void ServerSettings::validate() const {
    if (name.empty() || name.size()>128 || name.find_first_of("\r\n\t")!=std::string::npos)
        throw std::runtime_error("Server name must be 1-128 bytes and a single line");
    if (ip.empty() || ip.size()>255 || !port || slots<1 || slots>MAX_PLAYERS || teams<1 || teams>20 || teams>slots || bots>slots)
        throw std::runtime_error("Server requires a valid address/port, 1-32 slots, 1-20 teams (no more than slots), and bots <= slots");
    for (auto damage:{triggerDamage,boundsDamage,jabDamage,hookDamage})
        if (damage<0 || damage>10000) throw std::runtime_error("Damage must be between 0 and 10000");
    for (auto rate:{triggerBpm,boundsBpm})
        if (!std::isfinite(rate) || rate<1 || rate>60*TICK_RATE) throw std::runtime_error("Damage rates must be 1-3840 BPM");
}
ServerSettings loadServerSettings(const std::string& path) {
    auto t=toml::parse_file(path); ServerSettings s;
    for (const auto& [key,value]:t) if(key!="server" && key!="damage") throw std::runtime_error("Unknown server configuration table: "+std::string(key.str()));
    keys(t,"server",{"name","ip","port","slots","teams","bots","friendly_fire","honor_team_requests"});
    keys(t,"damage",{"trigger","trigger_bpm","out_of_bounds","out_of_bounds_bpm","jab","hook"});
    s.name=get<std::string>(t,"server","name",s.name);
    s.ip=get<std::string>(t,"server","ip",s.ip);
    s.port=integer(t,"server","port",s.port,65535);
    s.slots=integer(t,"server","slots",s.slots,MAX_PLAYERS);
    s.teams=integer(t,"server","teams",s.teams,20);
    s.bots=integer(t,"server","bots",s.bots,MAX_PLAYERS);
    s.honorTeamRequests=get<bool>(t,"server","honor_team_requests",s.honorTeamRequests);
    s.friendlyFire=get<bool>(t,"server","friendly_fire",s.friendlyFire);
    s.triggerDamage=integer(t,"damage","trigger",s.triggerDamage,10000);
    s.boundsDamage=integer(t,"damage","out_of_bounds",s.boundsDamage,10000);
    s.jabDamage=integer(t,"damage","jab",s.jabDamage,10000);
    s.hookDamage=integer(t,"damage","hook",s.hookDamage,10000);
    s.triggerBpm=get<double>(t,"damage","trigger_bpm",s.triggerBpm);
    s.boundsBpm=get<double>(t,"damage","out_of_bounds_bpm",s.boundsBpm);
    s.validate();return s;
}
ClientSettings loadClientSettings(const std::string& path) {
    auto t=toml::parse_file(path);ClientSettings s;
    for (const auto& [key,value]:t) if(key!="client" && key!="bindings") throw std::runtime_error("Unknown client configuration table: "+std::string(key.str()));
    keys(t,"client",{"name","ip","port","team","show_other_damage_numbers"});
    keys(t,"bindings",{"move_up","move_down","move_left","move_right","walk","jab","hook","scoreboard","debug"});
    for(std::size_t i=0;i<s.bindings.actions.size();++i) {
        const std::string action=i<BindingNames.size()?BindingNames[i]:"debug";
        const auto node=t["bindings"][action];
        if(!node)continue;
        auto& inputs=s.bindings.actions[i];inputs.clear();
        if(auto value=node.value<std::string>())inputs.push_back(parseBinding(*value));
        else if(auto array=node.as_array()) {
            for(const auto& entry:*array) {
                auto value=entry.value<std::string>();
                if(!value)throw std::runtime_error("bindings."+action+" must contain key names");
                inputs.push_back(parseBinding(*value));
            }
        }else throw std::runtime_error("bindings."+action+" must be a key name or array of key names");
    }
    s.showOtherDamageNumbers=get<bool>(t,"client","show_other_damage_numbers",s.showOtherDamageNumbers);
    s.team=integer(t,"client","team",s.team,20);
    s.name=get<std::string>(t,"client","name",s.name);s.ip=get<std::string>(t,"client","ip",s.ip);
    s.port=integer(t,"client","port",s.port,65535);
    if (s.name.empty() || s.name.size()>64 || s.ip.empty() || !s.port) throw std::runtime_error("Invalid client name, address, or port");
    return s;
}
void applySettings(const ServerSettings& s) {s.validate();activeSettings=s;setAttackDamage(s.jabDamage,s.hookDamage);}
void writeSettings(sf::Packet& p,const ServerSettings& s) {
    p<<s.ip<<s.port<<s.slots<<s.teams<<s.bots<<s.friendlyFire
     <<s.triggerDamage<<s.triggerBpm<<s.boundsDamage<<s.boundsBpm<<s.jabDamage<<s.hookDamage<<s.honorTeamRequests<<s.name;
}
bool readSettings(sf::Packet& p,ServerSettings& s) {
    ServerSettings value;
    if (!(p>>value.ip>>value.port>>value.slots>>value.teams>>value.bots>>value.friendlyFire
          >>value.triggerDamage>>value.triggerBpm>>value.boundsDamage>>value.boundsBpm>>value.jabDamage>>value.hookDamage>>value.honorTeamRequests)) return false;
    if(!p.endOfPacket() && !(p>>value.name))return false;
    try {value.validate();}catch(const std::exception&){return false;}
    s=value;return true;
}
sf::Color teamColor(std::uint32_t team,std::uint32_t count) {
    if (team>=count) return sf::Color(180,180,180,220);
    // Red, blue, then repeatedly bisect the largest remaining hue gap.
    std::vector<float> hues{0,240};
    while(hues.size()<count){
        auto sorted=hues;std::sort(sorted.begin(),sorted.end());float gap=-1,hue=0;
        for(std::size_t i=0;i<sorted.size();++i){float next=i+1<sorted.size()?sorted[i+1]:sorted[0]+360;
            if(next-sorted[i]>gap){gap=next-sorted[i];hue=std::fmod(sorted[i]+gap*.5f,360.f);}}
        hues.push_back(hue);
    }
    const float h=hues[team]/60.f,c=185.f,x=c*(1-std::abs(std::fmod(h,2.f)-1));
    sf::Vector3f rgb;
    if(h<1)rgb={c,x,0};else if(h<2)rgb={x,c,0};else if(h<3)rgb={0,c,x};
    else if(h<4)rgb={0,x,c};else if(h<5)rgb={x,0,c};else rgb={c,0,x};
    return sf::Color(static_cast<std::uint8_t>(rgb.x+70),static_cast<std::uint8_t>(rgb.y+70),static_cast<std::uint8_t>(rgb.z+70),220);
}
}
