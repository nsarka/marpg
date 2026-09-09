#pragma once

#include "common/logger.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ResourceManager
{
public:
    struct Frame {
        sf::IntRect rect{};
        float durationSeconds = 0.1f;
    };

    struct Clip {
        std::shared_ptr<sf::Texture> texture;
        std::vector<Frame> frames;
        bool looping = true;
    };

    struct AnimSet {
        std::array<Clip, 8> byFacing{};
        bool loaded = false;
    };

    struct CharacterAnimations {
        static constexpr std::size_t FacingCount = 8;
        static constexpr std::size_t AnimCount = 20;

        std::array<AnimSet, AnimCount> anims{};
    };

public:
    explicit ResourceManager(common::Logger& logger);

    bool loadFont(const std::string& id, const std::filesystem::path& path);
    bool loadFragmentShader(const std::string& id, const std::filesystem::path& path);

    // Loads the fantasy player sheets (15 frames x 8 directions).
    bool loadFantasyCharacter(const std::string& id,
                                 const std::filesystem::path& assetRoot);

    const sf::Font& getFont(const std::string& id) const;
    sf::Font& getFont(const std::string& id);

    const sf::Shader& getShader(const std::string& id) const;
    sf::Shader& getShader(const std::string& id);

    const CharacterAnimations& getCharacterAnimations(const std::string& id) const;
    CharacterAnimations& getCharacterAnimations(const std::string& id);

    bool hasFont(const std::string& id) const;
    bool hasShader(const std::string& id) const;
    bool hasCharacterAnimations(const std::string& id) const;

private:
    common::Logger& m_logger;

    std::unordered_map<std::string, sf::Font> m_fonts;
    std::unordered_map<std::string, sf::Shader> m_shaders;
    std::unordered_map<std::string, CharacterAnimations> m_characterAnimations;
};
