#pragma once
#include "common/attack_delivery.hpp"
#include "common/settings.hpp"
#include "client/tile_lighting.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <filesystem>

class SpellEffects {
public:
    explicit SpellEffects(const std::filesystem::path& folder) {
        for(int i=0;i<10;++i)if(!frames_[i].loadFromFile(folder/("Explosion"+std::to_string(i+1)+".png")))
            throw std::runtime_error("Cannot load spell explosion frames");
        for(int i=0;i<6;++i)if(!bolts_[i].loadFromFile(folder.parent_path()/"Lightning"/("Lightning_cycle"+std::to_string(i+1)+".png")))
            throw std::runtime_error("Cannot load lightning frames");
        for(int i=0;i<6;++i) {
            if(!chargeFire_[i].loadFromFile(folder.parent_path()/"Fire"/("Fire"+std::to_string(i+1)+".png")))
                throw std::runtime_error("Cannot load spell windup sprites");
        }
        for(int i=0;i<4;++i)
            if(!spots_[i].loadFromFile(folder.parent_path()/"Lightning"/("Lightning_spot"+std::to_string(i+1)+".png")))
                throw std::runtime_error("Cannot load lightning spot sprites");
    }
    void predictCast(std::size_t id,const common::PlayerState& player,common::AttackKind kind,sf::Vector2f position) {
        if(id>=warnings_.size() || !player.alive || player.stunTicks>0 || player.combatDebug.attack!=common::AttackKind::None)return;
        if((kind==common::AttackKind::Uppercut && player.explosionCooldown>0) ||
           (kind==common::AttackKind::Lightning && player.lightningCooldown>0))return;
        warnings_[id]=Warning{position,kind,0,player.attackSequence,true,0};
    }
    void observe(const std::vector<common::PlayerState>& players,float dt) {
        for(auto& effect:effects_)effect.age+=dt;
        effects_.erase(std::remove_if(effects_.begin(),effects_.end(),[](const auto& e){return e.age>=(e.lightning?.3f:.8f);}),effects_.end());
        for(std::size_t i=0;i<players.size() && i<seen_.size();++i) {
            const auto& p=players[i];
            if(!p.connected || !p.alive)warnings_[i].reset();
            if(!p.connected){seen_[i].reset();continue;}
            auto& warning=warnings_[i];
            const auto& combat=p.combatDebug;
            const bool casting=p.alive && (combat.attack==common::AttackKind::Uppercut || combat.attack==common::AttackKind::Lightning) &&
                combat.age<common::attackDescription(combat.attack).startupTicks+
                    (combat.attack==common::AttackKind::Lightning?common::attackDescription(combat.attack).activeTicks:0);
            if(warning) {warning->age+=dt;warning->pendingAge+=dt;}
            if(casting) {
                const float serverAge=combat.age*common::TICK_DT;
                if(!warning || warning->sequence!=p.attackSequence || warning->predicted)
                    warning=Warning{p.spellPosition,combat.attack,serverAge,p.attackSequence,false,0};
                else warning->age=std::max(warning->age,serverAge);
            } else if(warning && (!warning->predicted || p.attackSequence!=warning->sequence || warning->pendingAge>.5f)) {
                warning.reset();
            }
            if(seen_[i] && common::sequenceNewer(p.spellSequence,*seen_[i]))effects_.push_back({p.spellPosition,0,p.spellEffect==common::AttackKind::Lightning});
            seen_[i]=p.spellSequence;
        }
    }
    void addLights(const common::ClientLighting& settings) const {
        for(const auto& warning:warnings_)if(warning) {
            const bool lightning=warning->kind==common::AttackKind::Lightning;
            const float radius=lightning?common::activeSettings.lightning.radius:common::activeSettings.spell.radius;
            tileLighting.addLight(warning->position,lightning?settings.lightningSpot:settings.explosionWindup,radius);
        }
        for(const auto& effect:effects_) {
            const float radius=effect.lightning?common::activeSettings.lightning.radius:common::activeSettings.spell.radius;
            const float fade=1.f-effect.age/(effect.lightning?.3f:.8f);
            tileLighting.addLight(effect.position,effect.lightning?settings.lightningImpact:settings.explosionImpact,radius,fade);
        }
    }
    void drawWindups(sf::RenderTarget& target) const {
        for(const auto& entry:warnings_) {
            if(!entry)continue;
            const auto& w=*entry;
            const bool lightning=w.kind==common::AttackKind::Lightning;
            const auto& rules=lightning?common::activeSettings.lightning:common::activeSettings.spell;
            const float radius=rules.radius;
            const float duration=common::attackDescription(w.kind).startupTicks*common::TICK_DT;
            const float progress=std::clamp(w.age/duration,0.f,1.f);
            const int frame=static_cast<int>(w.age/.08f)%6;
            sf::Sprite charge(lightning?spots_[static_cast<int>(w.age/.08f)%4]:chargeFire_[frame]);
            const auto size=charge.getTexture().getSize();
            charge.setOrigin({size.x*.5f,lightning?size.y*.5f:float(size.y)});
            charge.setPosition(w.position);
            // Account for transparent padding; anchor flames at the target on the floor.
            const float scale=lightning?radius*2.f/48.f:std::min(.65f,radius/90.f);
            charge.setScale({scale,scale});
            charge.setColor(sf::Color(255,255,255,static_cast<std::uint8_t>(160+80*progress)));
            target.draw(charge);
        }
    }
    void draw(sf::RenderTarget& target) const {
        for(const auto& effect:effects_) {
            if(effect.lightning) {
                sf::Sprite bolt(bolts_[std::min(5,int(effect.age/.05f))]);
                const auto size=bolt.getTexture().getSize();
                bolt.setOrigin({size.x*.5f,float(size.y)});bolt.setScale({2.f,3.f});bolt.setPosition(effect.position);target.draw(bolt);
                continue;
            }
            sf::Sprite sprite(frames_[std::min(9,int(effect.age/.08f))]);
            sprite.setOrigin({128,128});const float scale=common::activeSettings.spell.radius/120.f*1.3f;sprite.setScale({scale,scale});sprite.setPosition(effect.position);
            target.draw(sprite);
        }
    }
private:
    struct Warning {
        sf::Vector2f position;
        common::AttackKind kind;
        float age;
        std::uint32_t sequence;
        bool predicted;
        float pendingAge;
    };
    std::array<std::optional<Warning>,common::MAX_PLAYERS> warnings_{};
    struct Effect {sf::Vector2f position;float age;bool lightning;};
    std::array<sf::Texture,10> frames_;
    std::array<sf::Texture,6> chargeFire_;
    std::array<sf::Texture,4> spots_;
    std::array<sf::Texture,6> bolts_;
    std::array<std::optional<std::uint32_t>,common::MAX_PLAYERS> seen_{};
    std::vector<Effect> effects_;
};
