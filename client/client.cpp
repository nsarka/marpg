#include "common/character_roster.hpp"
#include "loading_connection.hpp"
#include "common/common.hpp"
#include "common/collision_world.hpp"
#include "common/trigger_system.hpp"
#include "camera.hpp"
#include "player.hpp"
#include "client_connection.hpp"
#include "common/logger.hpp"
#include "common/map_layer.hpp"
#include "input_manager.hpp"
#include "hud.hpp"
#include "wall_occlusion.hpp"
#include "combat_debug.hpp"
#include "damage_numbers.hpp"
#include "kill_feed.hpp"
#include "scoreboard.hpp"
#include "spell_effects.hpp"
#include "attack_hud.hpp"
#include "sound_system.hpp"
#include "common/team_spawns.hpp"

#include <SFML/Graphics.hpp>

#include <cmath>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

namespace {
volatile std::sig_atomic_t quitRequested=0;
void requestQuit(int) { quitRequested=1; }
constexpr float kRemoteSnapshotInterval = 0.10f; // 10 Hz fake network updates

sf::Vector2f normalizeOrZero(sf::Vector2f v) {
    const float len2 = v.x * v.x + v.y * v.y;
    if (len2 <= 0.000001f) {
        return {0.f, 0.f};
    }

    const float len = std::sqrt(len2);
    return {v.x / len, v.y / len};
}

Player* chooseCameraTarget(std::vector<Player>& players, std::size_t localIndex) {
    if (localIndex < players.size()) {
        Player& local = players[localIndex];
        if (local.isConnected()) {
            return &local;
        }
    }

    for (Player& p : players) {
        if (p.isConnected() && p.isAlive()) {
            return &p;
        }
    }

    for (Player& p : players) {
        if (p.isConnected()) {
            return &p;
        }
    }

    return nullptr;
}

sf::RectangleShape makeOutlinedRect(const sf::FloatRect& rect,
                                    float thickness = 2.f,
                                    sf::Color color = sf::Color::Red)
{
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineThickness(thickness);
    shape.setOutlineColor(color);
    return shape;
}

} // namespace

int main(int argc, char**) {
    if(argc!=1) {
        std::cerr<<"Client does not accept command-line arguments. Edit client.toml instead.\n";
        return 1;
    }
    common::ClientSettings options;
    try {
        options=common::loadClientSettings("../client.toml");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    const auto& playerName=options.name;
    const auto& serverAddress=options.ip;
    std::signal(SIGINT,requestQuit);
    std::signal(SIGTERM,requestQuit);
    common::Logger logger;

    logger.info() << "Client started";

    sf::RenderWindow window(sf::VideoMode({(int)common::WINDOW_WIDTH, (int)common::WINDOW_HEIGHT}), "MARPG");
    window.setFramerateLimit(144);

    const std::filesystem::path assetRoot = "../assets/Fantasy tileset - 2D Isometric/Characters/Player";
    const std::filesystem::path fontPath  = "../assets/fonts/arial.ttf";
    const std::filesystem::path fragPath  = "../shaders/sprite_outline.frag";

    ResourceManager resources(logger);

    if (!resources.loadFont("ui", fontPath)) {
        return 1;
    }

    if (!resources.loadFragmentShader("sprite_outline", fragPath)) {
        return 1;
    }

    if (!resources.loadFragmentShader("player_occlusion", "../shaders/player_occlusion.frag")) return 1;

    for(const auto* character:common::CharacterNames)
        if(!resources.loadFantasyCharacter(character,assetRoot.parent_path()/character))return 1;

    // -------------------------------------------------------------------------
    // Client connection
    // -------------------------------------------------------------------------
    ClientConnection client_conn{logger};
    common::PlayerId myId = client_conn.connectToServer(serverAddress, playerName, options.port, options.team);
    if(myId == -1) {
        if(!client_conn.connectionError().empty()) {
            sf::Text message(resources.getFont("ui"),client_conn.connectionError()+"\n\nPress Escape or close this window to exit.",22);
            message.setPosition({30,60});
            while(window.isOpen() && !quitRequested) {
                while(auto event=window.pollEvent()) {
                    if(event->is<sf::Event::Closed>())window.close();
                    if(const auto* key=event->getIf<sf::Event::KeyPressed>();key && key->code==sf::Keyboard::Key::Escape)window.close();
                }
                window.clear(sf::Color(24,28,35));window.draw(message);window.display();
            }
        }
        return 1;
    }
    logger.log_info("Assigned player id ", myId);
    LoadingConnection loadingConnection(client_conn);

    // -------------------------------------------------------------------------
    // Level setup
    // -------------------------------------------------------------------------
    tmx::Map map;
    if (!map.load(common::mapPath())) {
        logger.log_error("Failed to load level: ", common::mapPath());
        return 1;
    }
    common::CollisionWorld collision;
    collision.load(common::mapPath());
    std::vector<sf::ConvexShape> collisionDebugShapes;
    for (const auto& points : collision.outlines()) {
        sf::ConvexShape shape(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) shape.setPoint(i, points[i]);
        shape.setFillColor(sf::Color(255, 70, 90, 55));
        shape.setOutlineColor(sf::Color(255, 80, 100));
        shape.setOutlineThickness(1.5f);
        collisionDebugShapes.push_back(std::move(shape));
    }
    common::TriggerSystem triggers;
    triggers.load(common::mapPath());
    for(const auto& region:triggers.regions().all()) {
        for(const auto& points:region->shape.outlines()) {
            sf::ConvexShape shape(points.size());
            for(std::size_t i=0;i<points.size();++i)shape.setPoint(i,points[i]);
            auto fill=region->color;fill.a=40;
            shape.setFillColor(fill);
            shape.setOutlineColor(region->color);
            shape.setOutlineThickness(1.5f);
            collisionDebugShapes.push_back(std::move(shape));
        }
    }
    common::TeamSpawns teamSpawns;
    teamSpawns.load(common::mapPath(),common::activeSettings.teams,collision,triggers);
    std::vector<sf::Text> levelLabels;
    for (const auto& mapLayer : map.getLayers()) {
        if (mapLayer->getType() != tmx::Layer::Type::Object || mapLayer->getName() != "Labels") continue;
        for (const auto& object : mapLayer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
            sf::Text label(resources.getFont("ui"), object.getName(), 18);
            const auto p = object.getPosition();
            const float scale = float(map.getTileSize().x) / (2.f * map.getTileSize().y);
            label.setPosition({(p.x - p.y) * scale, (p.x + p.y) * 0.5f});
            const auto bounds = label.getLocalBounds();
            label.setOrigin({bounds.position.x + bounds.size.x * 0.5f, 0.f});
            label.setFillColor(sf::Color(245, 235, 200));
            label.setOutlineColor(sf::Color(25, 25, 30));
            label.setOutlineThickness(2.f);
            levelLabels.push_back(std::move(label));
        }
    }
    auto layerIndex=[&](const std::string& name){
        for(std::size_t i=0;i<map.getLayers().size();++i)
            if(map.getLayers()[i]->getType()==tmx::Layer::Type::Tile && map.getLayers()[i]->getName()==name)return i;
        throw std::runtime_error("Missing tile layer: "+name);
    };
    MapLayer layerFloor(map, layerIndex("Floor"));
    MapLayer layerWalls(map, layerIndex("Walls"));
    WallOcclusion wallOcclusion(map, layerIndex("Walls"));
    //MapLayer layerTriggers(map, 9);
    layerWalls.update(sf::Time::Zero);
    layerFloor.update(sf::Time::Zero);
    logger.log_info("World bounds: ", layerFloor.getGlobalBounds());

    // -------------------------------------------------------------------------
    // Camera setup
    // -------------------------------------------------------------------------
    sf::FloatRect layerBounds = layerFloor.getGlobalBounds();
    Camera camera({common::WINDOW_WIDTH, common::WINDOW_HEIGHT});
    camera.setFollowSharpness(8.f);
    camera.setDeadZone({60.f, 40.f});
    camera.setWorldBounds(layerBounds);

    // -------------------------------------------------------------------------
    // Hud setup
    // -------------------------------------------------------------------------
    Hud hud(resources.getFont("ui"));
    hud.setWindowSize({common::WINDOW_WIDTH, common::WINDOW_HEIGHT});
    hud.setPlayerName(playerName);
    hud.setHealth(100.f, 100.f);
    hud.setStamina(100.f, 100.f);
    hud.setPingMs(0);

    // -------------------------------------------------------------------------
    // Input manager setup
    // -------------------------------------------------------------------------
    InputManager input{window, options.bindings};

    // -------------------------------------------------------------------------
    // Players: Complete Player for everybody, and an extra PlayerState for running
    // the local simulation for the client
    // -------------------------------------------------------------------------
    std::vector<Player> players{common::MAX_PLAYERS};
    bool shouldDrawWaitingForPlayers = true;
    int connectedPlayers = 0; // used for setting shouldDrawWaitingForPlayers
    std::vector<common::PlayerId> joined_players; // pushed when a player joined later than me in order to set his connected = true. popped after it gets set
    //common::PlayerState simulatedLocalPlayer{.connected = true, .alive = true};

    // Initialize world based on what the server tells us the state is the first time
    std::vector<common::PlayerState> newStates(common::MAX_PLAYERS);
    loadingConnection.finish(newStates,joined_players);
    client_conn.pumpNetwork(newStates, joined_players);
    for (int i = 0; i < common::MAX_PLAYERS; i++) {
        Player& p = players[i];

        p.state() = newStates[i];

        p.setCharacterAnimations(resources.getCharacterAnimations(common::CharacterNames[p.state().character]));
        p.setFont(resources.getFont("ui"));
        p.setSpriteScale({common::CharacterScale, common::CharacterScale});
        p.setOriginToFeet();
        p.setOcclusionShader(&resources.getShader("player_occlusion"));
        p.setInterpolationSharpness(14.f);
        p.setWalkSpeed(common::WALK_SPEED * 0.5f);
        p.setRunSpeed((common::WALK_SPEED + common::RUN_SPEED) * 0.5f);
        p.teleportTo(p.state().pos);

        p.setOutlineEnabled(true);
        p.setOutlineShader(&resources.getShader("sprite_outline"));
        p.setOutlineColor(common::teamColor(p.state().team,common::activeSettings.teams));
        p.setOutlineThickness(.5f);
    }

    // -------------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------------
    SoundSystem sounds("../assets/sound/Retro_Combat_FX");
    sf::Clock frameClock;
    sf::Clock mouseIdleClock;
    auto previousMousePosition=sf::Mouse::getPosition(window);
    float accumulator = 0.f;
    sf::Texture lightShadows,stairShadows;
    tileLighting.buildHeightField(lightShadows,stairShadows);
    SpellEffects spellEffects("../assets/sprites/Free Pixel Art Explosions/PNG/Explosion");
    DamageNumbers damageNumbers;
    KillFeed killFeed;
    std::optional<sf::Clock> shutdownDisplay;
    float teleportFlash = 0.f;
    constexpr float teleportFlashDuration = 0.22f;
    float damageFlash = 0.f;
    constexpr float damageFlashDuration = 0.35f;

    // -------------------------------------------------------------------------
    // Main loop
    // -------------------------------------------------------------------------
    while (window.isOpen() && !quitRequested) {
        // ---------------------------------------------------------------------
        // Render clock: real time between rendered frames
        // ---------------------------------------------------------------------
        const float renderDt = frameClock.restart().asSeconds();
        accumulator += renderDt;
        teleportFlash = std::max(0.f,teleportFlash-renderDt);
        damageFlash = std::max(0.f, damageFlash-renderDt);
        damageNumbers.update(renderDt);
        killFeed.update(renderDt);

        // ---------------------------------------------------------------------
        // Events
        // ---------------------------------------------------------------------
        input.updateSpellAvailability(newStates[myId]);
        input.handleEvents();
        if(!window.isOpen())break;

        // ---------------------------------------------------------------------
        // Network
        // ---------------------------------------------------------------------
        client_conn.pumpNetwork(newStates, joined_players);
        if(client_conn.shuttingDown()) {
            if(!shutdownDisplay)shutdownDisplay.emplace();
            window.setView(window.getDefaultView());window.clear(sf::Color(24,28,35));
            sf::Text message(resources.getFont("ui"),client_conn.shutdownReason()+"\nClosing game...",24);
            auto bounds=message.getLocalBounds();message.setOrigin(bounds.position+bounds.size*.5f);
            message.setPosition(window.getDefaultView().getSize()*.5f);message.setFillColor(sf::Color::White);
            window.draw(message);window.display();
            if(shutdownDisplay->getElapsedTime()>=sf::seconds(2))window.close();
            continue;
        }
        const auto mousePosition=sf::Mouse::getPosition(window);
        if(mousePosition!=previousMousePosition) {
            previousMousePosition=mousePosition;mouseIdleClock.restart();
        }
        const bool movementFacing=mouseIdleClock.getElapsedTime().asSeconds()>=options.mouseIdleSeconds;
        damageNumbers.observe(newStates, myId, options.showOtherDamageNumbers);
        if(client_conn.hasWorldSnapshot())killFeed.observe(client_conn.killEvents(),myId);
        input.updateSpellAvailability(newStates[myId]);
        spellEffects.observe(newStates,renderDt);
        if(!newStates[myId].alive)input.cancelCombatInput();
        sounds.observe(newStates, newStates[myId].pos);
        while(joined_players.size() > 0) {
            const common::PlayerId p = joined_players.back();
            joined_players.pop_back();
            players[p].state().connected = true;
        }
        //simulatedLocalPlayer = players[myId].state(); // Set the sim state to the authoratative state

        // ---------------------------------------------------------------------
        // Fixed simulation clock
        // ---------------------------------------------------------------------
        while (accumulator >= common::TICK_DT) {
            accumulator -= common::TICK_DT;

            // Build, send, then simulate the local player's input command
            common::InputCommand input_cmd = input.buildCommand(players[myId].renderPosition(), camera.view(), collision);
            input_cmd.movementFacing=movementFacing;
            if(input_cmd.spellPressed)
                spellEffects.predictCast(myId,newStates[myId],input_cmd.spellKind,input_cmd.spellTarget);
            client_conn.sendInput(myId, input_cmd);

            // -----------------------------------------------------------------
            // Local player simulation
            //
            // This is gameplay-side movement using a FIXED timestep.
            // In a real game, this would be input -> movement -> collision ->
            // prediction -> send input command to server.
            // -----------------------------------------------------------------
            // Player& localPlayer = players[myId];

            // {
            //     const float speed = input_cmd.sprint ? common::RUN_SPEED : common::WALK_SPEED;

            //     simulatedLocalPlayer.vel = input_cmd.move * speed;
            //     simulatedLocalPlayer.pos += simulatedLocalPlayer.vel * common::TICK_DT;

            //     localPlayer.applySnapshot(simulatedLocalPlayer);
            // }


            for (int i = 0; i < common::MAX_PLAYERS; i++) {
                auto& p = players[i];
                if (!newStates[i].connected) {
                    p.state().connected=false;
                    continue;
                }
                if (!p.state().connected) p.teleportTo(newStates[i].pos);
                if (i == myId && newStates[i].connected && newStates[i].health < p.state().health)
                    damageFlash = damageFlashDuration;
                if (i == myId && !p.isAlive() && newStates[i].alive) camera.snapTo(newStates[i].pos);
                if(i==myId && p.state().connected && newStates[i].teleportSequence!=p.state().teleportSequence) {
                    teleportFlash=teleportFlashDuration;
                    camera.snapTo(newStates[i].pos);
                    input.cancelCombatInput();
                }
                if(p.state().character!=newStates[i].character)
                    p.setCharacterAnimations(resources.getCharacterAnimations(common::CharacterNames[newStates[i].character]));
                p.applySnapshot(newStates[i]);
                p.setOutlineColor(common::teamColor(p.state().team,common::activeSettings.teams));
                //logger.log_info("Player ", i, "'s state: ", p.state());
            }
        }

        // ---------------------------------------------------------------------
        // Render/update phase
        //
        // This uses RENDER DT, not fixed simulation dt.
        // It is for:
        // - visual smoothing/interpolation
        // - animation
        // - camera smoothing
        // ---------------------------------------------------------------------
        connectedPlayers = 0;
        for (int i = 0; i < common::MAX_PLAYERS; i++) {
            auto& p = players[i];
            if (!p.state().connected) {
                continue;
            }
            connectedPlayers++;

            // Check if near another player
            p.setCombatIdle(false);
            for (int j = 0; j < common::MAX_PLAYERS; j++) {
                auto& p_other = players[j];
                if (!p_other.state().connected || j == i) {
                    continue;
                }
                if (common::distance(p.state().pos, p_other.state().pos) < 150.f) {
                    p.setCombatIdle(true);
                    p_other.setCombatIdle(true);    
                }
            }

            if(i==myId && window.hasFocus() && p.isAlive()) {
                const auto cursor=window.mapPixelToCoords(sf::Mouse::getPosition(window),camera.view());
                const auto& debug=p.state().combatDebug;
                const bool spellWindup=(debug.attack==common::AttackKind::Uppercut || debug.attack==common::AttackKind::Lightning) &&
                    debug.age<common::attackDescription(debug.attack).startupTicks;
                const auto delta=spellWindup?p.state().spellPosition-p.renderPosition():
                    movementFacing?p.state().vel:cursor-p.renderPosition();
                if(delta.length()>.001f)p.state().facing=delta.normalized();
            }
            p.update(renderDt);
        }

        // Me and the bot
        shouldDrawWaitingForPlayers = (connectedPlayers <= 2);

        // -------------------------------------------------------------------------
        // Update hud
        // -------------------------------------------------------------------------
        hud.setHealth(players[myId].state().health, 100.f);
        hud.setStamina(100, 100);
        //hud.setPingMs(client_conn.pingMs());
        if (shouldDrawWaitingForPlayers) {
            hud.setCenterMessage("Waiting for other players...");
        } else {
            hud.clearCenterMessage();
        }
        hud.setFps(1.f / std::max(renderDt, 0.0001f));

        // -------------------------------------------------------------------------
        // Update camera
        // -------------------------------------------------------------------------
        if (Player* target = chooseCameraTarget(players, myId)) {
            camera.follow(target->renderPosition());
        } else {
            camera.clearFollowTarget();
        }

        camera.update(renderDt);
        window.setView(camera.view());

        // ---------------------------------------------------------------------
        // Draw
        // ---------------------------------------------------------------------
        wallOcclusion.update(window.getSize(), camera.view());
        auto& visibilityShader = resources.getShader("player_occlusion");
        visibilityShader.setUniform("wallDepth", wallOcclusion.texture());
        visibilityShader.setUniform("renderSize", sf::Glsl::Vec2(window.getSize()));
        window.clear(sf::Color(30, 34, 42));

        tileLighting.lights.clear();
        for(std::size_t i=0;i<players.size();++i)if(players[i].state().connected && players[i].state().alive)
            tileLighting.addLight(players[i].renderPosition(),i<common::activeSettings.bots?options.lighting.bot:options.lighting.player);
        spellEffects.addLights(options.lighting);

        layerFloor.update(sf::seconds(renderDt));
        layerWalls.update(sf::seconds(renderDt));
        window.draw(layerFloor);
        spellEffects.drawWindups(window);
        window.draw(layerWalls);
        for (const auto& label : levelLabels) window.draw(label);
        //window.draw(layerTrigger);

        // Sort a separate view: player slots remain indexed by network ID.
        // Render positions include interpolation, matching the visible feet.
        std::vector<const Player*> characterDrawOrder;
        characterDrawOrder.reserve(players.size());
        for(const auto& player:players)
            if(player.isConnected())characterDrawOrder.push_back(&player);
        std::stable_sort(characterDrawOrder.begin(),characterDrawOrder.end(),[](const Player* a,const Player* b) {
            return a->renderPosition().y<b->renderPosition().y;
        });
        for(const auto* player:characterDrawOrder)window.draw(*player);

        if (input.collisionDebugEnabled()) {
            for (const auto& shape : collisionDebugShapes) window.draw(shape);
            sf::CircleShape feet(common::CollisionWorld::PlayerRadius);
            feet.setOrigin({common::CollisionWorld::PlayerRadius, common::CollisionWorld::PlayerRadius});
            feet.setFillColor(sf::Color::Transparent);
            feet.setOutlineColor(sf::Color(80, 255, 180));
            feet.setOutlineThickness(1.5f);
            for (auto& player : players) {
                if (!player.isConnected()) continue;
                feet.setPosition(player.state().pos);
                window.draw(feet);
            }
            drawCombatDebug(window, players, collision, resources.getFont("ui"));
            for(unsigned team=0;team<teamSpawns.groups().size();++team)for(auto point:teamSpawns.groups()[team]) {
                sf::CircleShape marker(12);marker.setOrigin({12,12});marker.setPosition(point);
                marker.setFillColor(sf::Color::Transparent);marker.setOutlineThickness(2);
                marker.setOutlineColor(common::teamColor(team,common::activeSettings.teams));window.draw(marker);
            }
        }

        spellEffects.draw(window);
        if(input.spellReady()) {
            const auto position=window.mapPixelToCoords(sf::Mouse::getPosition(window));
            const auto& spellRules=input.selectedSpell()==common::AttackKind::Lightning?common::activeSettings.lightning:common::activeSettings.spell;
            const float radius=spellRules.radius;
            sf::CircleShape area(radius);area.setOrigin({radius,radius});area.setPosition(position);
            const bool valid=common::spellTargetValid(newStates[myId],input.selectedSpell(),position,collision);
            area.setFillColor(valid?sf::Color(120,90,255,35):sf::Color(255,60,60,35));
            area.setOutlineColor(valid?sf::Color(180,140,255):sf::Color(255,60,60));area.setOutlineThickness(2.f);window.draw(area);
        }
        damageNumbers.draw(window, resources.getFont("ui"));

        // Draw debug rectangles in world space
        //window.draw(makeOutlinedRect(layerBounds, 2.f, sf::Color::Green)); // doesnt show up
        //window.draw(makeOutlinedRect(sf::FloatRect({300.f, 300.f}, {100.f, 100.f}), 2.f, sf::Color::Blue));

        // Draw hud & debug rectangles in screen space
        window.setView(window.getDefaultView()); // back to screen-space
        //hud.draw(window);
        //window.draw(makeOutlinedRect(sf::FloatRect({common::WINDOW_WIDTH / 2, common::WINDOW_HEIGHT / 2}, {120.f, 80.f}), 2.f, sf::Color::Black));

        if (input.collisionDebugEnabled()) {
            sf::Text legend(resources.getFont("ui"),
                "Combat: amber windup | red active | green hit | gray recovery\n"
                "Target lines: cyan clear | red wall blocked | gray outside arc\n"
                "Attack zones and feet show server positions", 13);
            legend.setPosition({12,12});
            legend.setOutlineColor(sf::Color::Black);legend.setOutlineThickness(1.f);
            window.draw(legend);
        }
        if(teleportFlash>0.f) {
            sf::RectangleShape flash(sf::Vector2f(window.getSize()));
            const float fade=teleportFlash/teleportFlashDuration;
            flash.setFillColor(sf::Color(150,45,235,static_cast<std::uint8_t>(95.f*fade*fade)));
            window.draw(flash);
        }
        if (damageFlash > 0.f) {
            sf::RectangleShape flash(sf::Vector2f(window.getSize()));
            const float fade = damageFlash / damageFlashDuration;
            flash.setFillColor(sf::Color(220, 15, 25, static_cast<std::uint8_t>(70.f*fade*fade)));
            window.draw(flash);
        }
        drawAttackHud(window,resources.getFont("ui"),newStates[myId],options.bindings);
        killFeed.draw(window,resources.getFont("ui"));
        if(input.keys().scoreboard)
            drawScoreboard(window,resources.getFont("ui"),newStates,myId);
        window.display();
    }

    return 0;
}
