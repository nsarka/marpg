#pragma once
#include "character_roster.hpp"
#include "common.hpp"
#include "player_class.hpp"
#include "trigger_regions.hpp"
#include <optional>

namespace common {
// Authored floor buttons share their exact geometry with rendering and debug views.
class CharacterSelect {
  public:
    struct Button {
        std::string label;
        std::uint8_t value;
        bool changesClass = false;
        sf::Vector2f center{};
        std::shared_ptr<const TriggerRegion> region;
    };
    explicit CharacterSelect(std::shared_ptr<TriggerRegions> regions) : regions_(std::move(regions)) {}
    void load(const tmx::Map& map) {
        buttons_.clear();
        for (const auto& layer : map.getLayers()) {
            if (layer->getType() != tmx::Layer::Type::Object || layer->getName() != "Triggers")
                continue;
            for (const auto& object : layer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
                const bool changesClass = object.getClass() == "ClassSelect";
                if (object.getClass() != "CharacterSelect" && !changesClass)
                    continue;
                std::optional<int> character;
                for (const auto& prop : object.getProperties())
                    if (prop.getName() == (changesClass ? "class" : "character") &&
                        prop.getType() == tmx::Property::Type::Int)
                        character = prop.getIntValue();
                const int count = changesClass ? int(PlayerClasses.size()) : int(CharacterNames.size());
                if (!character || *character < 0 || *character >= count || object.getName().empty())
                    throw std::runtime_error(
                        changesClass ? "ClassSelect requires a label and class index 0-3"
                                     : "CharacterSelect requires a label and character index 0-6");
                Button button{object.getName(), static_cast<std::uint8_t>(*character), changesClass};
                auto points = triggerObjectPoints(map, *layer, object);
                for (auto point : points)
                    button.center += point;
                button.center /= float(points.size());
                button.region = regions_->add(std::move(points), changesClass ? sf::Color(255, 200, 95)
                                                                              : sf::Color(105, 205, 245));
                buttons_.push_back(std::move(button));
            }
        }
    }
    const std::vector<Button>& buttons() const {
        return buttons_;
    }
    bool update(PlayerState& player) const {
        if (!player.connected || !player.alive || player.health <= 0)
            return false;
        for (const auto& button : buttons_)
            if (button.region->contains(player.pos)) {
                if (button.changesClass) {
                    const auto selected = static_cast<PlayerClass>(button.value);
                    if (player.playerClass == selected)
                        return false;
                    player.playerClass = selected;
                    return true;
                }
                if (player.character == button.value)
                    return false;
                player.character = button.value;
                return true;
            }
        return false;
    }

  private:
    std::shared_ptr<TriggerRegions> regions_;
    std::vector<Button> buttons_;
};
} // namespace common
