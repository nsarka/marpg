#include "client/player.hpp"
#include "client/resource_manager.hpp"
#include "common/character_roster.hpp"
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <stdexcept>
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    check(argc == 2, "Project root required");
    common::Logger logger;
    ResourceManager resources(logger);
    const auto root = std::filesystem::path(argv[1]);
    check(resources.loadFantasyCharacter("player",
                                         root / "assets/Fantasy tileset - 2D Isometric/Characters/Player"),
          "Fantasy player failed to load");
    for (const auto* name : common::CharacterNames) {
        ResourceManager characterResources(logger);
        check(characterResources.loadFantasyCharacter(
                  name, root / "assets/Fantasy tileset - 2D Isometric/Characters" / name),
              "Team character failed to load");
        for (std::size_t i = 0; i < common::CharacterAnimations.size(); ++i)
            for (const auto& clip : characterResources.getCharacterAnimations(name)
                                        .get(static_cast<common::CharacterAnimation>(i))
                                        .byFacing)
                check(clip.texture && clip.frames.size() == 15, "Team character animation incomplete");
    }
    const auto& animations = resources.getCharacterAnimations("player");
    {
        PlayerAnimation animation;
        animation.setAnimations(animations);
        using F = PlayerAnimation::Facing8;
        using A = common::CharacterAnimation;
        for (const auto& direction : std::array<std::pair<sf::Vector2f, F>, 4>{
                 {{{1, 0}, F::Dir6}, {{0, 1}, F::Dir8}, {{-1, 0}, F::Dir2}, {{0, -1}, F::Dir4}}}) {
            animation.face(direction.first);
            check(animation.facing() == direction.second, "Facing changed during controller extraction");
        }
        common::ServerSettings settings;
        settings.lightWindupSeconds = 1;
        common::PlayerState previous, state;
        previous.connected = state.connected = true;
        state.lastAttack = common::AttackKind::Light;
        state.attackSequence = 1;
        animation.observe(previous, state);
        animation.update(.5f, state, settings, false, 50, 140);
        check(animation.current() == A::Attack1 && animation.frame() < 7,
              "Controller ignored configured windup");
        previous = state;
        state.health -= 2;
        animation.observe(previous, state);
        check(animation.current() == A::TakeDamage, "Damage did not interrupt attack animation");
        previous = state;
        state.alive = false;
        state.health = 0;
        animation.observe(previous, state);
        check(animation.current() == A::Die, "Death animation missing");
        previous = state;
        state.alive = true;
        state.health = 100;
        animation.observe(previous, state);
        check(animation.current() == A::Idle && animation.frame() == 0, "Respawn did not reset animation");
        previous = state;
        state.lastAttack = common::AttackKind::Explosion;
        ++state.attackSequence;
        animation.observe(previous, state);
        check(animation.current() == A::Special1, "Explosion presentation is not Special1");
    }
    constexpr int rows[] = {3, 4, 5, 6, 7, 0, 1, 2};
    for (std::size_t i = 0; i < common::CharacterAnimations.size(); ++i) {
        const auto& set = animations.get(static_cast<common::CharacterAnimation>(i));
        check(set.loaded, "Missing animation state");
        for (unsigned facing = 0; facing < 8; ++facing) {
            const auto& clip = set.byFacing[facing];
            check(clip.texture && clip.frames.size() == 15, "Missing animation frames");
            check(clip.frames.front().rect.position.y == rows[facing] * 128, "Facing row incorrect");
            check(clip.frames.back().rect.position.x == 14 * 128, "Last frame missing");
            for (const auto& frame : clip.frames)
                check(frame.durationSeconds > 0 && frame.rect.size == sf::Vector2i{128, 128},
                      "Invalid frame");
        }
    }
    using A = common::CharacterAnimation;
    check(!animations.get(A::Die).byFacing[0].looping && !animations.get(A::TakeDamage).byFacing[0].looping,
          "Death or hurt loops");
    check(animations.get(A::Attack1).byFacing[0].texture != animations.get(A::Attack4).byFacing[0].texture,
          "Melee swings share a sheet");
    sf::RenderTexture preview({1024, 256});
    preview.clear(sf::Color(45, 45, 45));
    for (unsigned facing = 0; facing < 8; ++facing) {
        const auto& clip = animations.get(A::Idle).byFacing[facing];
        sf::Sprite sprite(*clip.texture, clip.frames[0].rect);
        sprite.setScale({2, 2});
        sprite.setOrigin({64, 88});
        sprite.setPosition({64.f + 128.f * facing, 180});
        preview.draw(sprite);
    }
    preview.display();
    check(preview.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path() /
                                                        "marpg-fantasy-player.png"),
          "Preview save failed");
}
