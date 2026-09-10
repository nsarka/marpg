#pragma once
#include "common/settings.hpp"
#include "ui_font.hpp"
#include <SFML/Graphics.hpp>
#include <optional>
class Nameplate {
    std::optional<sf::Text> text_;

  public:
    void setFont(const sf::Font &font, unsigned size) {
        text_.emplace(font, "", uiFontSize(size));
        text_->setFillColor(sf::Color::White);
        text_->setOutlineColor(sf::Color(20, 23, 28));
        text_->setOutlineThickness(1.f);
    }
    void setColor(sf::Color color) {
        if (text_)
            text_->setFillColor(color);
    }
    void update(const std::string &name, sf::Vector2f position) {
        if (!text_)
            return;
        text_->setString(name);
        const auto bounds = text_->getLocalBounds();
        text_->setPosition({position.x - bounds.position.x - bounds.size.x * .5f,
                            position.y - 112.f - bounds.position.y - bounds.size.y});
    }
    void draw(sf::RenderTarget &target, sf::RenderStates states, const common::PlayerState &player,
              sf::Vector2f position, unsigned teams) const {
        // Overhead UI stays readable and must not inherit the sprite occlusion shader.
        states.shader = nullptr;
        if (text_) {
            const auto bounds = text_->getGlobalBounds();
            sf::RectangleShape teamMark({8, 8});
            teamMark.setPosition({bounds.position.x - 13, bounds.position.y + (bounds.size.y - 8) * .5f});
            teamMark.setFillColor(common::teamColor(player.team, teams));
            target.draw(teamMark, states);
            target.draw(*text_, states);
        }
        constexpr float barWidth = 48.f;
        constexpr float barHeight = 5.f;
        const auto barPosition = position + sf::Vector2f{-barWidth * 0.5f, -94.f};
        sf::RectangleShape bar({barWidth, barHeight});
        bar.setPosition(barPosition);
        bar.setFillColor(sf::Color(180, 45, 50));
        bar.setOutlineColor(sf::Color(20, 23, 28));
        bar.setOutlineThickness(1.f);
        target.draw(bar, states);

        const float fraction = std::clamp(player.health / 100.f, 0.f, 1.f);
        if (fraction > 0.f) {
            sf::RectangleShape remaining({barWidth * fraction, barHeight});
            remaining.setPosition(barPosition);
            remaining.setFillColor(sf::Color(65, 210, 105));
            target.draw(remaining, states);
        }
    }
};
