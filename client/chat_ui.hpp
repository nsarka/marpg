#pragma once
#include "chat_input.hpp"
#include "common/attack_delivery.hpp"
#include "common/settings.hpp"
#include "common/slash_command.hpp"
#include "ui_font.hpp"
#include <SFML/Graphics.hpp>
#include <deque>
#include <map>

class ChatUI {
    struct Entry {
        common::ChatMessage message;
        float age = 0;
    };
    std::deque<Entry> history_;
    std::map<common::PlayerId, Entry> bubbles_;
    std::string notice_, announcement_;
    float announcementTime_ = 0;
    float noticeTime_ = 0;
    static sf::String utf8(const std::string& value) {
        return sf::String::fromUtf8(value.begin(), value.end());
    }
    static std::vector<sf::String> wrap(const sf::String& value, const sf::Font& font, unsigned size,
                                        float width) {
        std::vector<sf::String> lines;
        sf::String line;
        sf::Text measure(font, "", size);
        for (auto c : value) {
            sf::String next = line + c;
            measure.setString(next);
            if (!line.isEmpty() && measure.getLocalBounds().size.x > width) {
                // Prefer the last space, but still wrap a long unbroken word.
                std::size_t space = line.getSize();
                while (space > 0 && line[space - 1] != ' ')
                    --space;
                if (space) {
                    lines.push_back(line.substring(0, space - 1));
                    line = line.substring(space) + c;
                } else {
                    lines.push_back(line);
                    line = c;
                }
            } else
                line = std::move(next);
        }
        lines.push_back(line);
        return lines;
    }

  public:
    ChatInput input;
    explicit ChatUI(const common::KeyBindings& bindings = {}) : input(bindings) {}
    void add(const common::ChatMessage& message) {
        history_.push_back({message});
        if (history_.size() > 100)
            history_.pop_front();
        if (message.kind == common::ChatKind::Announcement) {
            announcement_ = message.text;
            announcementTime_ = 8;
        }
        if (!message.dead && message.kind == common::ChatKind::Player) {
            auto previous = bubbles_.find(message.sender);
            if (previous == bubbles_.end() ||
                common::sequenceNewer(message.sequence, previous->second.message.sequence))
                bubbles_[message.sender] = {message};
        }
    }
    void clearBubbles() {
        bubbles_.clear();
    }
    void clear() {
        history_.clear();
        bubbles_.clear();
        noticeTime_ = announcementTime_ = 0;
    }
    void localMessage(const std::string& text) {
        common::ChatMessage message;
        message.kind = common::ChatKind::System;
        message.text = text;
        add(message);
    }
    bool localCommand(const std::string& text) {
        auto command = common::slashCommand(text);
        if (!command)
            return false;
        if (command->name == "team" || command->name == "name")
            return false;
        if (command->name == "clear" && command->argument.empty())
            clear();
        else if (command->name == "help" && command->argument.empty()) {
            localMessage("/help - show commands; /clear - clear local chat");
            localMessage("/team <number|auto> - change team (server balance rules apply)");
            localMessage("/name <new name> - change your name for this session");
        } else
            localMessage("Unknown command or arguments. Use /help.");
        return true;
    }
    void deliveryFailed() {
        notice_ = "Message could not be delivered. Please try again.";
        noticeTime_ = 8;
    }
    void update(float seconds, const std::vector<common::PlayerState>& players) {
        for (auto& entry : history_)
            entry.age += seconds;
        for (auto it = bubbles_.begin(); it != bubbles_.end();) {
            const auto& m = it->second.message;
            it->second.age += seconds;
            if (it->second.age > 6 || m.sender >= players.size() || !players[m.sender].connected ||
                !players[m.sender].alive || players[m.sender].deaths != m.deaths ||
                players[m.sender].name != m.name)
                it = bubbles_.erase(it);
            else
                ++it;
        }
        noticeTime_ = std::max(0.f, noticeTime_ - seconds);
        announcementTime_ = std::max(0.f, announcementTime_ - seconds);
    }
    void drawAbove(sf::RenderTarget& target, const sf::Font& font, common::PlayerId id,
                   const common::PlayerState& state, sf::Vector2f feet) const {
        auto found = bubbles_.find(id);
        if (found == bubbles_.end() || !state.connected || !state.alive)
            return;
        const auto& entry = found->second;
        if (state.deaths != entry.message.deaths || state.name != entry.message.name)
            return;
        const unsigned size = uiFontSize(14);
        const auto lines = wrap(utf8(entry.message.text), font, size, 240);
        const float spacing = 19;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            sf::Text text(font, lines[i], size);
            const auto bounds = text.getLocalBounds();
            text.setOrigin(bounds.position + sf::Vector2f{bounds.size.x * .5f, bounds.size.y});
            text.setPosition(feet + sf::Vector2f{0, -145.f - float(lines.size() - 1 - i) * spacing});
            const auto alpha = static_cast<std::uint8_t>(255 * std::clamp(6.f - entry.age, 0.f, 1.f));
            text.setFillColor(sf::Color(255, 255, 255, alpha));
            text.setOutlineColor(sf::Color(15, 15, 20, alpha));
            text.setOutlineThickness(1);
            target.draw(text);
        }
    }
    void draw(sf::RenderTarget& target, const sf::Font& font, unsigned teams) const {
        const auto view = target.getView();
        if (announcementTime_ > 0) {
            float y = 24;
            for (const auto& line :
                 wrap(utf8(announcement_), font, uiFontSize(24), target.getView().getSize().x - 80)) {
                sf::Text text(font, line, uiFontSize(24));
                auto bounds = text.getLocalBounds();
                text.setOrigin({bounds.position.x + bounds.size.x * .5f, 0});
                text.setPosition({target.getView().getSize().x * .5f, y});
                text.setFillColor(sf::Color(255, 235, 160));
                text.setOutlineColor(sf::Color::Black);
                text.setOutlineThickness(2);
                target.draw(text);
                y += 32;
            }
        }

        const float width = std::min(480.f, view.getSize().x - 32.f);
        const float bottom = view.getSize().y - 85.f;
        const unsigned size = uiFontSize(15);
        constexpr float spacing = 22;
        struct Line {
            sf::String text;
            sf::Color color;
            std::size_t nameStart = 0, nameLength = 0;
            sf::Color nameColor;
        };
        std::vector<Line> lines;
        for (const auto& entry : history_) {
            if (!input.open() && entry.age > 15)
                continue;
            auto color = entry.message.kind == common::ChatKind::Player
                             ? common::teamColor(entry.message.team, teams)
                             : sf::Color(255, 230, 160);
            // Text remains readable even for dark team colors.
            color.r = std::max<std::uint8_t>(130, color.r);
            color.g = std::max<std::uint8_t>(130, color.g);
            color.b = std::max<std::uint8_t>(130, color.b);
            const auto value = utf8(common::chatLine(entry.message));
            const bool player = entry.message.kind == common::ChatKind::Player;
            const std::size_t nameStart = entry.message.dead ? 7 : 0;
            const auto nameEnd = nameStart + utf8(entry.message.name).getSize();
            std::size_t offset = 0;
            for (const auto& line : wrap(value, font, size, width - 20)) {
                // Wrapping can drop a space; retain positions in the original text.
                offset = value.find(line, offset);
                Line rendered{line, player ? sf::Color::White : color};
                const auto start = std::max(offset, nameStart);
                const auto end = std::min(offset + line.getSize(), nameEnd);
                if (player && start < end) {
                    rendered.nameStart = start - offset;
                    rendered.nameLength = end - start;
                    rendered.nameColor = color;
                }
                lines.push_back(rendered);
                offset += line.getSize();
            }
        }
        if (noticeTime_ > 0)
            for (const auto& line : wrap(utf8(notice_), font, size, width - 20))
                lines.push_back({line, sf::Color(255, 150, 150)});
        const auto first = lines.size() > 7 ? lines.size() - 7 : 0;
        float inputHeight = 0;
        std::vector<sf::String> draft;
        if (input.open()) {
            draft = wrap(sf::String("> ") + input.draft() + "_", font, size, width - 20);
            inputHeight = draft.size() * spacing + 8;
        }
        if (lines.empty() && !input.open())
            return;
        const float height = (lines.size() - first) * spacing + inputHeight + 16;
        sf::RectangleShape background({width, height});
        background.setPosition({16, bottom - height});
        background.setFillColor(sf::Color(15, 18, 25, input.open() ? 205 : 140));
        target.draw(background);
        float y = bottom - height + 8;
        auto drawLine = [&](const Line& line) {
            sf::Text text(font, line.text, size);
            text.setPosition({26, y});
            text.setFillColor(line.color);
            text.setOutlineColor(sf::Color(10, 10, 15));
            text.setOutlineThickness(1);
            target.draw(text);
            if (line.nameLength) {
                const auto position = text.findCharacterPos(line.nameStart);
                text.setString(line.text.substring(line.nameStart, line.nameLength));
                text.setPosition(position);
                text.setFillColor(line.nameColor);
                target.draw(text);
            }
            y += spacing;
        };
        for (auto i = first; i < lines.size(); ++i)
            drawLine(lines[i]);
        if (input.open()) {
            y += 8;
            for (const auto& line : draft)
                drawLine({line, sf::Color::White});
        }
    }
};
