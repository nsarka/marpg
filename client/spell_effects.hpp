#pragma once
#include "attack_presentation.hpp"
#include "client/rendering/tile_lighting.hpp"
#include "common/attack_delivery.hpp"
#include "common/beam_attack.hpp"
#include "common/settings.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <filesystem>

class SpellEffects {
  public:
    explicit SpellEffects(const std::filesystem::path& folder,
                          const common::ServerSettings& rules = common::ServerSettings{},
                          const common::CollisionWorld* walls = nullptr)
        : rules_(rules), walls_(walls) {
        for (int i = 0; i < 10; ++i)
            if (!frames_[i].loadFromFile(folder / ("Explosion" + std::to_string(i + 1) + ".png")))
                throw std::runtime_error("Cannot load spell explosion frames");
        for (int i = 0; i < 6; ++i)
            if (!bolts_[i].loadFromFile(folder.parent_path() / "Lightning" /
                                        ("Lightning_cycle" + std::to_string(i + 1) + ".png")))
                throw std::runtime_error("Cannot load lightning frames");
        for (int i = 0; i < 6; ++i) {
            if (!chargeFire_[i].loadFromFile(folder.parent_path() / "Fire" /
                                             ("Fire" + std::to_string(i + 1) + ".png")))
                throw std::runtime_error("Cannot load spell windup sprites");
        }
        for (int i = 0; i < 4; ++i)
            if (!spots_[i].loadFromFile(folder.parent_path() / "Lightning" /
                                        ("Lightning_spot" + std::to_string(i + 1) + ".png")))
                throw std::runtime_error("Cannot load lightning spot sprites");
    }
    void predictCast(std::size_t id, const common::PlayerState& player, common::AttackKind kind,
                     sf::Vector2f position) {
        if (id >= warnings_.size() || !player.alive || player.stunTicks > 0 || player.dodgeTicks > 0 ||
            player.combatDebug.attack != common::AttackKind::None)
            return;
        if ((kind == common::AttackKind::Explosion && player.explosionCooldown > 0) ||
            (kind == common::AttackKind::Lightning && player.lightningCooldown > 0) ||
            (kind == common::AttackKind::Beam && player.beamCooldown > 0))
            return;
        warnings_[id] = Warning{position, kind, 0, player.attackSequence, true, 0};
        warnings_[id]->origin = player.pos;
    }
    void observe(const std::vector<common::PlayerState>& players, float dt) {
        for (auto& effect : effects_)
            effect.age += dt;
        effects_.erase(
            std::remove_if(effects_.begin(), effects_.end(),
                           [](const auto& e) { return e.age >= ((e.lightning || e.beam) ? .3f : .8f); }),
            effects_.end());
        for (std::size_t i = 0; i < players.size() && i < seen_.size(); ++i) {
            const auto& p = players[i];
            if (!p.connected || !p.alive || p.dodgeTicks)
                warnings_[i].reset();
            if (!p.connected) {
                seen_[i].reset();
                continue;
            }
            auto& warning = warnings_[i];
            const auto& combat = p.combatDebug;
            const bool casting =
                p.alive && (common::isSpell(combat.attack)) &&
                combat.elapsedTicks < common::attackDescription(combat.attack, rules_).startupTicks +
                                          (combat.attack == common::AttackKind::Lightning
                                               ? common::attackDescription(combat.attack, rules_).activeTicks
                                               : 0);
            if (warning) {
                warning->age += dt;
                warning->pendingAge += dt;
            }
            if (casting) {
                const float serverAge = combat.elapsedTicks * common::TICK_DT;
                if (!warning || warning->sequence != p.attackSequence || warning->predicted)
                    warning = Warning{p.spellPosition, combat.attack, serverAge, p.attackSequence, false, 0};
                else
                    warning->age = std::max(warning->age, serverAge);
            } else if (warning && (!warning->predicted || p.attackSequence != warning->sequence ||
                                   warning->pendingAge > .5f)) {
                warning.reset();
            }
            if (warning)
                warning->origin = p.pos;
            if (seen_[i] && common::sequenceNewer(p.spellSequence, *seen_[i]))
                effects_.push_back(
                    {p.spellPosition, 0,
                     client::attackPresentation(p.spellEffect).effect == client::SpellVisual::Lightning,
                     p.spellEffect == common::AttackKind::Beam, p.spellOrigin});
            seen_[i] = p.spellSequence;
        }
    }
    void addLights(TileLighting& tileLighting, const common::ClientLighting& settings) const {
        for (const auto& warning : warnings_)
            if (warning) {
                if (warning->kind == common::AttackKind::Beam) {
                    tileLighting.addLight(warning->origin, settings.lightningSpot, 50);
                    continue;
                }
                const bool lightning = warning->kind == common::AttackKind::Lightning;
                const float radius =
                    common::attackDescription(
                        lightning ? common::AttackKind::Lightning : common::AttackKind::Explosion, rules_)
                        .hitRadius;
                tileLighting.addLight(warning->position,
                                      lightning ? settings.lightningSpot : settings.explosionWindup, radius);
            }
        for (const auto& effect : effects_) {
            if (effect.beam) {
                const float fade = 1.f - effect.age / .3f;
                for (int i = 0; i <= 4; ++i)
                    tileLighting.addLight(effect.origin + (effect.position - effect.origin) * (i / 4.f),
                                          settings.lightningImpact, 70, fade);
                continue;
            }
            const float radius =
                common::attackDescription(
                    effect.lightning ? common::AttackKind::Lightning : common::AttackKind::Explosion, rules_)
                    .hitRadius;
            const float fade = 1.f - effect.age / (effect.lightning ? .3f : .8f);
            tileLighting.addLight(effect.position,
                                  effect.lightning ? settings.lightningImpact : settings.explosionImpact,
                                  radius, fade);
        }
    }
    void drawWindups(sf::RenderTarget& target) const {
        for (const auto& entry : warnings_) {
            if (!entry)
                continue;
            const auto& w = *entry;
            if (w.kind == common::AttackKind::Beam) {
                drawBeam(target, w.origin, warningEndpoint(w), float(rules_.beam.effectRadius), .6f, true);
                continue;
            }
            const bool lightning = w.kind == common::AttackKind::Lightning;
            const auto rules = common::attackDescription(w.kind, rules_);
            const float radius = rules.hitRadius;
            const float duration = common::attackDescription(w.kind, rules_).startupTicks * common::TICK_DT;
            const float progress = std::clamp(w.age / duration, 0.f, 1.f);
            const int frame = static_cast<int>(w.age / .08f) % 6;
            sf::Sprite charge(lightning ? spots_[static_cast<int>(w.age / .08f) % 4] : chargeFire_[frame]);
            const auto size = charge.getTexture().getSize();
            charge.setOrigin({size.x * .5f, lightning ? size.y * .5f : float(size.y)});
            charge.setPosition(w.position);
            // Account for transparent padding; anchor flames at the target on the floor.
            const float scale = lightning ? radius * 2.f / 48.f : std::min(.65f, radius / 90.f);
            charge.setScale({scale, scale});
            charge.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(160 + 80 * progress)));
            target.draw(charge);
        }
    }
    void draw(sf::RenderTarget& target) const {
        for (const auto& effect : effects_) {
            if (effect.beam) {
                const sf::Vector2f height{0, -32};
                drawBeam(target, effect.origin + height, effect.position + height,
                         float(rules_.beam.effectRadius), 1.f - effect.age / .3f, false);
                continue;
            }
            if (effect.lightning) {
                sf::Sprite bolt(bolts_[std::min(5, int(effect.age / .05f))]);
                const auto size = bolt.getTexture().getSize();
                bolt.setOrigin({size.x * .5f, float(size.y)});
                bolt.setScale({2.f, 3.f});
                bolt.setPosition(effect.position);
                target.draw(bolt);
                continue;
            }
            sf::Sprite sprite(frames_[std::min(9, int(effect.age / .08f))]);
            sprite.setOrigin({128, 128});
            const float scale =
                common::attackDescription(common::AttackKind::Explosion, rules_).hitRadius / 120.f * 1.3f;
            sprite.setScale({scale, scale});
            sprite.setPosition(effect.position);
            target.draw(sprite);
        }
    }
    static void drawBeam(sf::RenderTarget& target, sf::Vector2f from, sf::Vector2f end, float radius,
                         float opacity, bool warning) {
        const auto ray = end - from;
        if (ray.length() < .001f)
            return;
        const auto angle = sf::radians(std::atan2(ray.y, ray.x));
        auto stroke = [&](float width, sf::Color color) {
            color.a = static_cast<std::uint8_t>(color.a * std::clamp(opacity, 0.f, 1.f));
            sf::RectangleShape line({ray.length(), width});
            line.setOrigin({0, width * .5f});
            line.setPosition(from);
            line.setRotation(angle);
            line.setFillColor(color);
            target.draw(line);
        };
        stroke(radius * 4, sf::Color(70, 160, 255, warning ? 25 : 60));
        stroke(radius * 2, sf::Color(75, 205, 255, warning ? 100 : 220));
        stroke(warning ? 1.f : std::max(2.f, radius * .4f), sf::Color(225, 250, 255, 255));
    }

  private:
    common::ServerSettings rules_;
    const common::CollisionWorld* walls_;
    struct Warning {
        sf::Vector2f position;
        common::AttackKind kind;
        float age;
        std::uint32_t sequence;
        bool predicted;
        float pendingAge;
        sf::Vector2f origin{};
    };
    sf::Vector2f warningEndpoint(const Warning& warning) const {
        if (walls_)
            return common::beamEndpoint(warning.origin, warning.position, *walls_,
                                        float(rules_.beam.castRange));
        const auto ray = warning.position - warning.origin;
        return ray.length() > .001f ? warning.origin + ray.normalized() * float(rules_.beam.castRange)
                                    : warning.origin;
    }
    std::array<std::optional<Warning>, common::MAX_PLAYERS> warnings_{};
    struct Effect {
        sf::Vector2f position;
        float age;
        bool lightning;
        bool beam = false;
        sf::Vector2f origin{};
    };
    std::array<sf::Texture, 10> frames_;
    std::array<sf::Texture, 6> chargeFire_;
    std::array<sf::Texture, 4> spots_;
    std::array<sf::Texture, 6> bolts_;
    std::array<std::optional<std::uint32_t>, common::MAX_PLAYERS> seen_{};
    std::vector<Effect> effects_;
};
