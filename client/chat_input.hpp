#pragma once
#include "common/chat.hpp"
#include "common/keybindings.hpp"
#include <SFML/Window/Event.hpp>
#include <optional>
#include <set>
#include <utility>

class ChatInput {
    common::KeyBindings bindings_;
    bool open_ = false, suppressOpeningText_ = false;
    sf::String draft_;
    std::set<int> held_;
    std::optional<std::string> submitted_;

  public:
    explicit ChatInput(const common::KeyBindings& bindings = {}) : bindings_(bindings) {}
    bool open() const {
        return open_;
    }
    const sf::String& draft() const {
        return draft_;
    }
    std::optional<std::string> takeSubmitted() {
        return std::exchange(submitted_, {});
    }
    bool consume(const sf::Event& event) {
        if (event.is<sf::Event::Closed>())
            return false;
        if (event.is<sf::Event::FocusLost>()) {
            open_ = false;
            draft_.clear();
            held_.clear();
            suppressOpeningText_ = false;
            return false;
        }
        std::optional<int> pressed;
        const auto* key = event.getIf<sf::Event::KeyPressed>();
        if (key)
            pressed = int(key->code);
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>())
            pressed = common::mouseBinding(mouse->button);
        bool fresh = pressed && held_.insert(*pressed).second;
        if (const auto* released = event.getIf<sf::Event::KeyReleased>()) {
            held_.erase(int(released->code));
            suppressOpeningText_ = false;
        }
        if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
            held_.erase(common::mouseBinding(released->button));
        if (!open_) {
            const auto& bindings = bindings_[common::Action::Chat];
            if (pressed && std::find(bindings.begin(), bindings.end(), *pressed) != bindings.end()) {
                if (fresh) {
                    open_ = true;
                    draft_.clear();
                    suppressOpeningText_ = key != nullptr;
                }
                return true;
            }
            return false;
        }
        if (key) {
            suppressOpeningText_ = false;
            if (key->code == sf::Keyboard::Key::Enter && fresh) {
                auto bytes = draft_.toUtf8();
                auto clean = common::cleanChatText(std::string(bytes.begin(), bytes.end()));
                if (!clean.empty())
                    submitted_ = std::move(clean);
                open_ = false;
                draft_.clear();
            } else if (key->code == sf::Keyboard::Key::Escape) {
                open_ = false;
                draft_.clear();
            } else if (key->code == sf::Keyboard::Key::Backspace && !draft_.isEmpty()) {
                draft_.erase(draft_.getSize() - 1, 1);
            }
        } else if (const auto* entered = event.getIf<sf::Event::TextEntered>()) {
            if (suppressOpeningText_) {
                suppressOpeningText_ = false;
                return true;
            }
            auto candidate = draft_ + entered->unicode;
            if (entered->unicode >= 32 && entered->unicode != 127 &&
                candidate.getSize() <= common::ChatMaxCharacters &&
                candidate.toUtf8().size() <= common::ChatMaxBytes)
                draft_ = std::move(candidate);
        }
        return true; // Typing owns gameplay keyboard and mouse input until closed.
    }
};
