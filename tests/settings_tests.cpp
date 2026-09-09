#include "common/settings.hpp"
#include "common/team_spawns.hpp"
#include "common/combat_system.hpp"
#include <filesystem>
#include <fstream>
#include <set>
#include <iostream>
#include <chrono>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==2,"Project path required");std::filesystem::path root=argv[1];
    auto temp=std::filesystem::temp_directory_path()/("marpg-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".toml");
    {
        {std::ofstream file(temp);file<<"[light_player]\nradius=650\nintensity=0.4\nheight=120\nred=0.2\ndirectionality=0.1\nfalloff_exponent=2\n[light_bot]\nenabled=false\n[light_lightning_impact]\nradius_multiplier=3\n";}
        const auto settings=common::loadClientSettings(temp.string());
        check(settings.lighting.player.radius==650 && settings.lighting.player.intensity==.4 && settings.lighting.player.height==120 && settings.lighting.player.red==.2 && settings.lighting.player.directionality==.1 && settings.lighting.player.falloffExponent==2,"Light settings not loaded");
        check(!settings.lighting.bot.enabled && settings.lighting.lightningImpact.radiusMultiplier==3 && settings.lighting.explosionWindup.intensity==.55,"Independent lighting defaults failed");
        for(const auto& invalid:{"[light_player]\nradius=-1\n","[light_bot]\nheight=nan\n","[light_explosion_impact]\nred=2\n","[light_lightning_spot]\ndirectionality=2\n","[light_player]\nfalloff_exponent=0\n","[light_bot]\nenabled=1\n","[light_player]\nraduis=2\n"}) {
            {std::ofstream file(temp);file<<invalid;}
            bool rejected=false;try{common::loadClientSettings(temp.string());}catch(...){rejected=true;}
            check(rejected,"Invalid light setting accepted");
        }
        std::filesystem::remove(temp);
    }
    for(const auto& text:{"[server]\nrespawn_seconds = -1\n","[server]\nslots = -1\n","[trigger_damage]\ntrigger_interval_seconds = 0\n","[server]\nport = 70000\n","[server]\nteams = \"two\"\n","[server]\nslts = 12\n","[server\n"}) {
        {std::ofstream file(temp);file<<text;}
        bool rejected=false;try{common::loadServerSettings(temp.string());}catch(const std::exception&){rejected=true;}
        std::filesystem::remove(temp);check(rejected,"Invalid TOML setting was silently accepted");
    }
    {
        {std::ofstream file(temp);file<<"[jab]\nwindup_seconds=0.5\ndamage_min=23\ndamage_max=23\nrange=25.0\ncone_degrees=45.0\n[hook]\ndamage_min=37\ndamage_max=37\nrange=95.0\nwindup_seconds=0.75\n";}
        auto timing=common::loadServerSettings(temp.string());
        check(timing.jabWindup==.5 && timing.hookWindup==.75,"Melee windups not loaded");
        sf::Packet wire;common::writeSettings(wire,timing);common::ServerSettings copy;
        check(common::readSettings(wire,copy) && copy.jabWindup==.5 && copy.hookWindup==.75,"Melee windups not synchronized");
        check(copy.jabDamage==23 && copy.hookDamage==37 && copy.jabDamageMin==23 && copy.hookDamageMin==37 && copy.jabConeDegrees==45 && copy.jabRange==25 && copy.hookRange==95,"Melee damage/range not loaded and synchronized");
        common::applySettings(copy);
        check(common::attackDescription(common::AttackKind::Jab).startupTicks==32 &&
              common::attackDescription(common::AttackKind::Hook).startupTicks==48,"Configured windup ticks incorrect");
        common::PlayerState a,b;a.connected=b.connected=true;a.pos={0,0};b.pos={30,0};
        common::CombatState combat;common::CollisionWorld walls;
        common::startAttack(a,combat,common::AttackKind::Jab,{1,0});
        for(int i=0;i<31;++i)common::updateAttack(a,combat,{&a,&b},walls);
        check(b.health==100,"Configured jab hit before windup");
        common::updateAttack(a,combat,{&a,&b},walls);
        check(b.health==100,"Jab hit beyond configured range");
        b.pos={24,0};common::updateAttack(a,combat,{&a,&b},walls);
        check(b.health==77,"Configured jab range/damage not applied");
        for(const auto& text:{"[jab]\ncone_degrees=0\n","[hook]\ncone_degrees=361\n","[jab]\ndamage_min=50\ndamage_max=10\n","[jab]\nrange=0\n","[hook]\nrange=inf\n","[hook]\ndamage_min=-1\n","[jab]\nwindup_seconds=-1\n","[hook]\nwindup_seconds=61\n","[jab]\nwindup_seconds=nan\n"}) {
            {std::ofstream file(temp);file<<text;}
            bool rejected=false;try{common::loadServerSettings(temp.string());}catch(...){rejected=true;}
            check(rejected,"Invalid melee windup accepted");
        }
        check(common::ServerSettings{}.map=="demo","Wrong default map");
    common::applySettings(common::ServerSettings{});
        check(common::attackDescription(common::AttackKind::Jab).startupTicks==16 &&
              common::attackDescription(common::AttackKind::Hook).startupTicks==24,"Default windups changed");
        std::filesystem::remove(temp);
    }
    {std::ofstream file(temp);file<<"[trigger_damage]\ntrigger_interval_seconds=0.25\nout_of_bounds_interval_seconds=1.5\n[server]\nrespawn_seconds=4.0\n";}
    const auto intervals=common::loadServerSettings(temp.string());
    check(intervals.respawnSeconds==4.0 && intervals.triggerInterval==.25 && intervals.boundsInterval==1.5 &&
          common::damageInterval(intervals.triggerInterval)==16 && common::damageInterval(intervals.boundsInterval)==96,
          "Seconds-based damage intervals not parsed or converted correctly");
    std::filesystem::remove(temp);
    auto settings=common::loadServerSettings((root/"server.toml").string());
    auto client=common::loadClientSettings((root/"client.toml").string());
    // User-editable TOML files may intentionally override the built-in defaults.
    settings=common::ServerSettings{};client=common::ClientSettings{};
    check(client.mouseIdleSeconds==3,"Mouse idle timeout default changed");
    {std::ofstream file(temp);file<<"[client]\nmouse_idle_seconds=1.5\n";}
    check(common::loadClientSettings(temp.string()).mouseIdleSeconds==1.5,"Mouse idle timeout not loaded");
    std::filesystem::remove(temp);
    check(client.showOtherDamageNumbers,"Third-party damage numbers should default to enabled");
    check(settings.honorTeamRequests && client.team==0,"Default team request policy changed");
    check(settings.teams==2 && settings.slots==20 && client.port==54000 && client.ip=="147.182.213.239","Default files not loaded");
    check(settings.name=="Rick's Funhaus","Default server name changed");
    {std::ofstream file(temp);file<<"[server]\nname=\"Custom Arena\"\n";}
    check(common::loadServerSettings(temp.string()).name=="Custom Arena","Server name not loaded");
    std::filesystem::remove(temp);
    check(settings.botAI,"Bot AI must default to enabled");
    {std::ofstream file(temp);file<<"[server]\nbot_ai=false\n";}
    check(!common::loadServerSettings(temp.string()).botAI,"Cannot disable bot AI in TOML");
    std::filesystem::remove(temp);
    settings.botAI=false;
    {std::ofstream file(temp);file<<"[spell]\ndamage_min=9\ndamage_max=17\nradius=80.0\nrange=420.0\ncooldown_seconds=2.5\n[lightning]\ndamage_interval_seconds=0.2\nduration_seconds=4.0\n";}
    const auto spells=common::loadServerSettings(temp.string());std::filesystem::remove(temp);
    check(spells.spell.damageMin==9 && spells.spell.radius==80 && spells.lightning.interval==.2,"Spell config not loaded");
    settings.spell=spells.spell;settings.lightning=spells.lightning;
    auto invalidSpell=settings;invalidSpell.spell.damageMin=100;
    bool badRange=false;try{invalidSpell.validate();}catch(...){badRange=true;}check(badRange,"Inverted damage range accepted");
    settings.name="Custom Arena";settings.map="arena";
    settings.port=55001;settings.slots=12;settings.teams=3;settings.triggerDamage=7;settings.triggerInterval=1.0;
    settings.boundsDamage=9;settings.boundsInterval=.25;settings.jabDamageMin=settings.jabDamage=27;settings.hookDamageMin=settings.hookDamage=40;
    settings.honorTeamRequests=true;
    sf::Packet packet;common::writeSettings(packet,settings);common::ServerSettings received;
    check(common::readSettings(packet,received) && received.spell.damageMax==17 && received.lightning.interval==.2 && !received.botAI && received.map=="arena" && received.name=="Custom Arena" && received.port==55001 && received.teams==3 && received.boundsInterval==.25 && received.honorTeamRequests,"Settings wire roundtrip failed");
    common::applySettings(settings);
    check(common::attackDescription(common::AttackKind::Jab).damage==27 && common::attackDescription(common::AttackKind::Hook).damage==40,"Combat ignores config");
    common::TriggerSystem triggers;triggers.load((root/"assets/tiled/legacy_demo.tmx").string());
    common::PlayerState player;player.connected=true;player.pos={-512,1024};
    triggers.update(0,player);check(player.health==93,"Configured trigger entry damage");
    for(int i=0;i<63;++i)triggers.update(0,player);check(player.health==93,"Trigger beat too early");
    triggers.update(0,player);check(player.health==86,"Configured trigger interval");
    player.pos={128,-1};triggers.update(0,player);check(player.health==77,"Configured bounds entry damage");
    for(int i=0;i<16;++i)triggers.update(0,player);check(player.health==68,"Configured bounds interval");
    // Use an isolated map so editing the demo does not change spawn-loader tests.
    const auto spawnMap=temp.string()+".tmx";
    common::CollisionWorld walls;common::TriggerSystem safeFloor;
    for(unsigned pointCount:{1u,2u,7u,20u,35u}) {
        {
            std::ofstream file(spawnMap);
            file<<"<map version='1.10' orientation='isometric' width='100' height='100' tilewidth='256' tileheight='128'><objectgroup name='Spawns'>";
            for(unsigned i=0;i<pointCount;++i)
                file<<"<object id='"<<i+1<<"' x='"<<i*256<<"' y='128'><point/></object>";
            file<<"</objectgroup></map>";
        }
        for(unsigned teams=1;teams<=std::min(20u,pointCount);++teams) {
            common::TeamSpawns spawns;spawns.load(spawnMap,teams,walls,safeFloor);
            unsigned count=0;std::set<std::uint32_t> colors;
            std::vector<common::PlayerState> occupants;
            for(unsigned team=0;team<teams;++team) {
                check(!spawns.groups()[team].empty(),"Team without spawns");count+=spawns.groups()[team].size();
                colors.insert(common::teamColor(team,teams).toInteger());
                auto position=spawns.choose(team,occupants,walls,safeFloor);check(position.has_value(),"No safe team spawn");
                common::PlayerState occupant;occupant.connected=true;occupant.team=team;occupant.pos=*position;occupants.push_back(occupant);
            }
            check(count==pointCount && colors.size()==teams,"Spawns/colors not unique or complete");
        }
        if(pointCount<20) {
            bool rejected=false;try {common::TeamSpawns spawns;spawns.load(spawnMap,pointCount+1,walls,safeFloor);}catch(...) {rejected=true;}
            check(rejected,"Too few spawns for teams must be rejected");
        }
    }
    {std::ofstream file(spawnMap);file<<"<map version='1.10' orientation='isometric' width='1' height='1' tilewidth='256' tileheight='128'><objectgroup name='Spawns'/></map>";}
    bool noSpawns=false;try {common::TeamSpawns spawns;spawns.load(spawnMap,1,walls,safeFloor);}catch(...) {noSpawns=true;}
    check(noSpawns,"Empty spawn layer must be rejected");
    std::filesystem::remove(spawnMap);
    check(common::teamColor(0,2)==sf::Color(255,70,70,220),"Red changed");
    check(common::teamColor(1,2)==sf::Color(70,70,255,220),"Blue wrong");
    common::PlayerState attacker,target;attacker.connected=target.connected=true;attacker.team=target.team=0;
    attacker.pos={0,0};target.pos={30,0};common::CombatState combat;common::CollisionWorld empty;
    common::startAttack(attacker,combat,common::AttackKind::Jab,{1,0});
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,{&attacker,&target},empty);
    check(target.health==100,"Friendly fire should be disabled");
    auto friendly=settings;friendly.friendlyFire=true;common::applySettings(friendly);
    common::startAttack(attacker,combat,common::AttackKind::Jab,{1,0});
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,{&attacker,&target},empty);
    check(target.health==73,"Configured friendly fire did not enable teammate damage");
    target.health=100;common::applySettings(settings);
    target.team=1;common::startAttack(attacker,combat,common::AttackKind::Hook,{1,0});
    for(int i=0;i<64;++i)common::updateAttack(attacker,combat,{&attacker,&target},empty);
    check(target.health==60,"Enemy/configured hook damage");
    common::respawn(target,combat,{10,20});check(target.team==1,"Respawn changed team");
    auto invalid=settings;invalid.teams=0;bool failed=false;try{invalid.validate();}catch(...){failed=true;}check(failed,"Invalid config accepted");
    {
        common::ServerSettings rules;rules.teams=2;rules.honorTeamRequests=false;
        std::vector<common::PlayerState> members(3);
        check(common::chooseTeam(members,rules,2)==1,"Balanced request must break a tie");
        members[0].connected=true;members[0].team=1;
        check(common::chooseTeam(members,rules,2)==0,"Request must not bypass balancing by default");
        rules.honorTeamRequests=true;
        check(common::chooseTeam(members,rules,2)==1,"Server override must honor crowded team");
        check(common::chooseTeam(members,rules,0)==0,"Automatic assignment must still balance");
        check(common::chooseTeam(members,rules,20)==0,"Unavailable team must fall back to automatic");
        auto clientPath=std::filesystem::temp_directory_path()/"marpg-client-team-test.toml";
        {std::ofstream file(clientPath);file<<"[client]\nteam=2\nshow_other_damage_numbers=true\n";}
        check(common::loadClientSettings(clientPath.string()).team==2 && common::loadClientSettings(clientPath.string()).showOtherDamageNumbers,"Client team preference not loaded");
        {std::ofstream file(clientPath);file<<"[bindings]\nmove_up=\"Up\"\njab=[\"Space\",\"Mouse_Left\"]\ndebug=[]\n";}
        const auto bindings=common::loadClientSettings(clientPath.string()).bindings;
        check(bindings.actions[0][0]==common::parseBinding("up") && bindings.actions[5].size()==2 && bindings.actions[8].empty(),"Custom bindings not parsed");
        check(bindings.actions[4].size()==2 && bindings.actions[7][0]==common::parseBinding("Tab"),"Missing bindings lost defaults");
        for(const auto& invalid:{"[bindings]\njab=42\n","[bindings]\njab=\"typo\"\n","[bindings]\nunknown=\"W\"\n"}) {
            {std::ofstream file(clientPath);file<<invalid;}
            bool rejected=false;try{common::loadClientSettings(clientPath.string());}catch(...){rejected=true;}
            check(rejected,"Invalid keybinding accepted");
        }
        {std::ofstream file(clientPath);file<<"[client]\nteam=21\n";}
        bool rejected=false;try{common::loadClientSettings(clientPath.string());}catch(...){rejected=true;}
        std::filesystem::remove(clientPath);check(rejected,"Invalid client team accepted");
    }
    check(common::ServerSettings{}.map=="demo","Wrong default map");
    common::applySettings(common::ServerSettings{});
    std::cout<<"PASS: TOML, settings sync, damage/intervals, teams, friendly fire, variable spawn counts\n";
}
