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
    if(map.empty() || map.size()>64 || map.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
        throw std::runtime_error("map must be a map name in assets/tiled without a path or .tmx extension");
    if (name.empty() || name.size()>128 || name.find_first_of("\r\n\t")!=std::string::npos)
        throw std::runtime_error("Server name must be 1-128 bytes and a single line");
    if (ip.empty() || ip.size()>255 || !port || slots<1 || slots>MAX_PLAYERS || teams<1 || teams>20 || teams>slots || bots>slots)
        throw std::runtime_error("Server requires a valid address/port, 1-32 slots, 1-20 teams (no more than slots), and bots <= slots");
    if(jabDamageMin<0 || jabDamageMin>jabDamage || hookDamageMin<0 || hookDamageMin>hookDamage)
        throw std::runtime_error("Melee damage must satisfy 0 <= min <= max <= 10000");
    if(!std::isfinite(respawnSeconds) || respawnSeconds<0 || respawnSeconds>600)
        throw std::runtime_error("respawn_seconds must be between 0 and 600");
    if(!std::isfinite(damageStunSeconds) || damageStunSeconds<0 || damageStunSeconds>60)
        throw std::runtime_error("damage_stun_seconds must be between 0 and 60");
    for(double cone:{jabConeDegrees,hookConeDegrees})
        if(!std::isfinite(cone) || cone<=0 || cone>360)
            throw std::runtime_error("Melee cone_degrees must be greater than 0 and at most 360");
    for(double range:{jabRange,hookRange})
        if(!std::isfinite(range) || range<=0 || range>10000)
            throw std::runtime_error("Jab/hook range must be greater than 0 and at most 10000");
    for(double windup:{jabWindup,hookWindup})
        if(!std::isfinite(windup) || windup<0 || windup>60)
            throw std::runtime_error("Jab/hook windup must be between 0 and 60 seconds");
    for(const auto& spell:{this->spell,lightning}) {
        if(spell.damageMin<0 || spell.damageMax<spell.damageMin || spell.damageMax>10000)
            throw std::runtime_error("Spell damage range must satisfy 0 <= min <= max <= 10000");
        for(double v:{spell.radius,spell.range,spell.windup,spell.cooldown,spell.duration,spell.interval})
            if(!std::isfinite(v) || v<0 || v>10000)throw std::runtime_error("Invalid spell size or timing");
        if(spell.radius<=0 || spell.range<=0 || spell.interval<1.0/TICK_RATE || spell.duration>60 || spell.windup>60 || spell.cooldown>600)
            throw std::runtime_error("Spell radius/range must be positive; interval >= 1 tick; duration/windup <= 60s; cooldown <= 600s");
    }
    for (auto damage:{triggerDamage,boundsDamage,jabDamage,hookDamage})
        if (damage<0 || damage>10000) throw std::runtime_error("Damage must be between 0 and 10000");
    for (auto interval:{triggerInterval,boundsInterval})
        if (!std::isfinite(interval) || interval<1.0/TICK_RATE || interval>60) throw std::runtime_error("Damage intervals must be between 0.015625 and 60 seconds");
}
ServerSettings loadServerSettings(const std::string& path) {
    auto t=toml::parse_file(path); ServerSettings s;
    for (const auto& [key,value]:t) if(key!="server" && key!="trigger_damage" && key!="spell" && key!="lightning" && key!="jab" && key!="hook") throw std::runtime_error("Unknown server configuration table: "+std::string(key.str()));
    keys(t,"server",{"map","name","ip","port","slots","teams","bots","bot_ai","friendly_fire","honor_team_requests","damage_stun_seconds","respawn_seconds"});
    keys(t,"trigger_damage",{"trigger","trigger_interval_seconds","out_of_bounds","out_of_bounds_interval_seconds"});
    keys(t,"jab",{"windup_seconds","damage_min","damage_max","range","cone_degrees"});
    keys(t,"hook",{"windup_seconds","damage_min","damage_max","range","cone_degrees"});
    s.jabDamageMin=integer(t,"jab","damage_min",s.jabDamageMin,10000);
    s.hookDamageMin=integer(t,"hook","damage_min",s.hookDamageMin,10000);
    s.jabConeDegrees=get<double>(t,"jab","cone_degrees",s.jabConeDegrees);
    s.hookConeDegrees=get<double>(t,"hook","cone_degrees",s.hookConeDegrees);
    s.jabRange=get<double>(t,"jab","range",s.jabRange);
    s.hookRange=get<double>(t,"hook","range",s.hookRange);
    s.jabWindup=get<double>(t,"jab","windup_seconds",s.jabWindup);
    s.hookWindup=get<double>(t,"hook","windup_seconds",s.hookWindup);
    s.respawnSeconds=get<double>(t,"server","respawn_seconds",s.respawnSeconds);
    s.damageStunSeconds=get<double>(t,"server","damage_stun_seconds",s.damageStunSeconds);
    s.map=get<std::string>(t,"server","map",s.map);
    s.name=get<std::string>(t,"server","name",s.name);
    s.ip=get<std::string>(t,"server","ip",s.ip);
    s.port=integer(t,"server","port",s.port,65535);
    s.slots=integer(t,"server","slots",s.slots,MAX_PLAYERS);
    s.teams=integer(t,"server","teams",s.teams,20);
    s.bots=integer(t,"server","bots",s.bots,MAX_PLAYERS);
    s.botAI=get<bool>(t,"server","bot_ai",s.botAI);
    s.honorTeamRequests=get<bool>(t,"server","honor_team_requests",s.honorTeamRequests);
    s.friendlyFire=get<bool>(t,"server","friendly_fire",s.friendlyFire);
    s.triggerDamage=integer(t,"trigger_damage","trigger",s.triggerDamage,10000);
    s.boundsDamage=integer(t,"trigger_damage","out_of_bounds",s.boundsDamage,10000);
    s.jabDamage=integer(t,"jab","damage_max",s.jabDamage,10000);
    s.hookDamage=integer(t,"hook","damage_max",s.hookDamage,10000);
    s.triggerInterval=get<double>(t,"trigger_damage","trigger_interval_seconds",s.triggerInterval);
    s.boundsInterval=get<double>(t,"trigger_damage","out_of_bounds_interval_seconds",s.boundsInterval);
    for(const auto& section:{"spell","lightning"}) {
        keys(t,section,{"damage_min","damage_max","radius","range","windup_seconds","cooldown_seconds","duration_seconds","damage_interval_seconds"});
        auto& v=std::string(section)=="spell"?s.spell:s.lightning;
        v.damageMin=integer(t,section,"damage_min",v.damageMin,10000);v.damageMax=integer(t,section,"damage_max",v.damageMax,10000);
        v.radius=get<double>(t,section,"radius",v.radius);v.range=get<double>(t,section,"range",v.range);
        v.windup=get<double>(t,section,"windup_seconds",v.windup);v.cooldown=get<double>(t,section,"cooldown_seconds",v.cooldown);
        v.duration=get<double>(t,section,"duration_seconds",v.duration);v.interval=get<double>(t,section,"damage_interval_seconds",v.interval);
    }
    s.validate();return s;
}
ClientSettings loadClientSettings(const std::string& path) {
    auto t=toml::parse_file(path);ClientSettings s;
    for (const auto& [key,value]:t) if(key!="client" && key!="bindings" && key!="light_player" && key!="light_bot" && key!="light_explosion_windup" && key!="light_explosion_impact" && key!="light_lightning_spot" && key!="light_lightning_impact") throw std::runtime_error("Unknown client configuration table: "+std::string(key.str()));
    auto light=[&](const std::string& section,LightSettings& value) {
        keys(t,section,{"enabled","radius","intensity","height","red","green","blue","radius_multiplier","directionality","falloff_exponent"});
        if(t[section]["enabled"] && !t[section]["enabled"].is_boolean())throw std::runtime_error(section+".enabled must be true or false");
        value.enabled=get<bool>(t,section,"enabled",value.enabled);
        auto number=[&](const char* key,double& destination,double minimum,double maximum) {
            destination=get<double>(t,section,key,destination);
            if(!std::isfinite(destination) || destination<minimum || destination>maximum)
                throw std::runtime_error(section+"."+key+" must be between "+std::to_string(minimum)+" and "+std::to_string(maximum));
        };
        number("radius",value.radius,0,10000);number("intensity",value.intensity,0,10);
        number("height",value.height,1,2000);number("radius_multiplier",value.radiusMultiplier,0,20);
        number("red",value.red,0,1);number("green",value.green,0,1);number("blue",value.blue,0,1);
        number("directionality",value.directionality,0,1);number("falloff_exponent",value.falloffExponent,.1,8);
    };
    light("light_player",s.lighting.player);light("light_bot",s.lighting.bot);
    light("light_explosion_windup",s.lighting.explosionWindup);light("light_explosion_impact",s.lighting.explosionImpact);
    light("light_lightning_spot",s.lighting.lightningSpot);light("light_lightning_impact",s.lighting.lightningImpact);
    keys(t,"client",{"name","ip","port","team","show_other_damage_numbers","mouse_idle_seconds"});
    keys(t,"bindings",{"move_up","move_down","move_left","move_right","walk","jab","hook","scoreboard","debug","spell","lightning"});
    for(std::size_t i=0;i<s.bindings.actions.size();++i) {
        const std::string action=i<BindingNames.size()?BindingNames[i]:(i==8?"debug":i==9?"spell":"lightning");
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
    s.mouseIdleSeconds=get<double>(t,"client","mouse_idle_seconds",s.mouseIdleSeconds);
    if(!std::isfinite(s.mouseIdleSeconds) || s.mouseIdleSeconds<0 || s.mouseIdleSeconds>600)throw std::runtime_error("mouse_idle_seconds must be between 0 and 600");
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
     <<s.triggerDamage<<s.triggerInterval<<s.boundsDamage<<s.boundsInterval<<s.jabDamage<<s.hookDamage<<s.honorTeamRequests<<s.name<<s.botAI;
    for(const auto& v:{s.spell,s.lightning})p<<v.damageMin<<v.damageMax<<v.radius<<v.range<<v.windup<<v.cooldown<<v.duration<<v.interval;
    p<<s.jabWindup<<s.hookWindup;
    p<<s.jabRange<<s.hookRange;
    p<<s.jabDamageMin<<s.hookDamageMin<<s.jabConeDegrees<<s.hookConeDegrees;
    p<<s.damageStunSeconds;
    p<<s.respawnSeconds<<s.map;
}
bool readSettings(sf::Packet& p,ServerSettings& s) {
    ServerSettings value;
    if (!(p>>value.ip>>value.port>>value.slots>>value.teams>>value.bots>>value.friendlyFire
          >>value.triggerDamage>>value.triggerInterval>>value.boundsDamage>>value.boundsInterval>>value.jabDamage>>value.hookDamage>>value.honorTeamRequests)) return false;
    if(!p.endOfPacket() && !(p>>value.name))return false;
    if(!p.endOfPacket() && !(p>>value.botAI))return false;
    if(!p.endOfPacket())for(auto* v:{&value.spell,&value.lightning})
        if(!(p>>v->damageMin>>v->damageMax>>v->radius>>v->range>>v->windup>>v->cooldown>>v->duration>>v->interval))return false;
    if(!p.endOfPacket() && !(p>>value.jabWindup>>value.hookWindup))return false;
    if(!p.endOfPacket() && !(p>>value.jabRange>>value.hookRange))return false;
    value.jabDamageMin=value.jabDamage;value.hookDamageMin=value.hookDamage;
    if(!p.endOfPacket() && !(p>>value.jabDamageMin>>value.hookDamageMin>>value.jabConeDegrees>>value.hookConeDegrees))return false;
    if(!p.endOfPacket() && !(p>>value.damageStunSeconds))return false;
    if(!p.endOfPacket() && !(p>>value.respawnSeconds))return false;
    if(!(p>>value.map))return false;
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
