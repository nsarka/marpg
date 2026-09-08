#pragma once
#include "common/common.hpp"
#include "common/attack_delivery.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <random>

class DamageNumbers {
public:
    struct Number {
        int amount;
        sf::Vector2f origin;
        bool incoming;
        float age=0;
    };
    static constexpr float Lifetime=0.9f;
    explicit DamageNumbers(unsigned seed=std::random_device{}()) : random_(seed) {}
    void observe(const std::vector<common::PlayerState>& players, common::PlayerId local, bool showOtherDamageNumbers=true) {
        for (std::size_t victim=0;victim<players.size() && victim<seen_.size();++victim) {
            if (!players[victim].connected) continue;
            for (const auto& event : players[victim].damageEvents) {
                if (seen_[victim] && !common::sequenceNewer(event.sequence,*seen_[victim])) continue;
                seen_[victim]=event.sequence;
                if (victim!=local && event.source!=static_cast<std::int32_t>(local) &&
                    !(showOtherDamageNumbers && event.source>=0)) continue;
                if (event.amount<=0) continue;
                std::uniform_real_distribution<float> x(-12.f,12.f),y(-6.f,6.f);
                if (numbers_.size()==64) numbers_.erase(numbers_.begin());
                numbers_.push_back({event.amount,event.contact+sf::Vector2f{x(random_),-14.f+y(random_)},victim==local});
            }
        }
    }
    void update(float dt) {
        for (auto& number : numbers_) number.age+=std::max(0.f,dt);
        numbers_.erase(std::remove_if(numbers_.begin(),numbers_.end(),[](const auto& number){
            return number.age>=Lifetime;
        }),numbers_.end());
    }
    static sf::Vector2f position(const Number& number) {
        return number.origin+sf::Vector2f{0,-42.f*number.age/Lifetime};
    }
    static sf::Color color(const Number& number) {
        const auto alpha=static_cast<std::uint8_t>(255.f*std::clamp(1.f-number.age/Lifetime,0.f,1.f));
        return number.incoming ? sf::Color(255,75,80,alpha) : sf::Color(255,255,255,alpha);
    }
    static std::string label(const Number& number) { return "-" + std::to_string(number.amount); }
    void draw(sf::RenderTarget& target, const sf::Font& font) const {
        for (const auto& number : numbers_) {
            sf::Text text(font,label(number),21);
            text.setStyle(sf::Text::Bold);
            const auto tint=color(number);
            text.setFillColor(tint);
            text.setOutlineColor(sf::Color(15,18,22,tint.a));text.setOutlineThickness(1.5f);
            const auto bounds=text.getLocalBounds();
            text.setOrigin({bounds.position.x+bounds.size.x*.5f,bounds.position.y+bounds.size.y});
            text.setPosition(position(number));target.draw(text);
        }
    }
    const std::vector<Number>& numbers() const {return numbers_;}
private:
    std::mt19937 random_;
    std::array<std::optional<std::uint32_t>,common::MAX_PLAYERS> seen_{};
    std::vector<Number> numbers_;
};
