#pragma once
#include "common/attack_delivery.hpp"
#include "common/settings.hpp"
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
    }
    void observe(const std::vector<common::PlayerState>& players,float dt) {
        for(auto& effect:effects_)effect.age+=dt;
        effects_.erase(std::remove_if(effects_.begin(),effects_.end(),[](const auto& e){return e.age>=(e.lightning?.3f:.8f);}),effects_.end());
        for(std::size_t i=0;i<players.size() && i<seen_.size();++i) {
            const auto& p=players[i];
            if(!p.connected){seen_[i].reset();continue;}
            if(seen_[i] && common::sequenceNewer(p.spellSequence,*seen_[i]))effects_.push_back({p.spellPosition,0,p.spellEffect==common::AttackKind::Lightning});
            seen_[i]=p.spellSequence;
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
    struct Effect {sf::Vector2f position;float age;bool lightning;};
    std::array<sf::Texture,10> frames_;
    std::array<sf::Texture,6> bolts_;
    std::array<std::optional<std::uint32_t>,common::MAX_PLAYERS> seen_{};
    std::vector<Effect> effects_;
};
