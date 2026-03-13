#include "resource_manager.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::size_t kIdle        = 0;
    constexpr std::size_t kWalk        = 1;
    constexpr std::size_t kRunning     = 2;
    constexpr std::size_t kJump        = 3;
    constexpr std::size_t kRunningJump = 4;
    constexpr std::size_t kRunningRoll = 5;
    constexpr std::size_t kFightIdle   = 6;
    constexpr std::size_t kBlock       = 7;
    constexpr std::size_t kLeftJab     = 8;
    constexpr std::size_t kRightHook   = 9;
    constexpr std::size_t kUppercut    = 10;
    constexpr std::size_t kCombo       = 11;
    constexpr std::size_t kDamaged     = 12;
    constexpr std::size_t kDie         = 13;
    constexpr std::size_t kPickUp      = 14;
    constexpr std::size_t kPickUpLow   = 15;
    constexpr std::size_t kCarrying    = 16;
    constexpr std::size_t kPull        = 17;
    constexpr std::size_t kPush        = 18;
    constexpr std::size_t kTalking     = 19;
}

ResourceManager::ResourceManager(common::Logger& logger)
    : m_logger(logger)
{
}

bool ResourceManager::loadFont(const std::string& id,
                               const std::filesystem::path& path)
{
    sf::Font font;
    if (!font.openFromFile(path)) {
        m_logger.log_error("Failed to load font: ", path);
        return false;
    }

    m_fonts[id] = std::move(font);
    return true;
}

bool ResourceManager::loadFragmentShader(const std::string& id,
                                         const std::filesystem::path& path)
{
    sf::Shader shader;
    if (!shader.loadFromFile(path, sf::Shader::Type::Fragment)) {
        m_logger.log_error("Failed to load fragment shader: ", path);
        return false;
    }

    m_shaders[id] = std::move(shader);
    return true;
}

void ResourceManager::parseFramesFromJson(const std::filesystem::path& jsonPath,
                                          std::vector<Frame>& outFrames)
{
    std::ifstream ifs(jsonPath);
    if (!ifs) {
        throw std::runtime_error("Failed to open JSON file: " + jsonPath.string());
    }

    nlohmann::json j;
    ifs >> j;

    if (!j.contains("frames") || !j["frames"].is_array()) {
        throw std::runtime_error("JSON missing frames array: " + jsonPath.string());
    }

    outFrames.clear();

    for (const auto& item : j["frames"]) {
        const auto& fr = item.at("frame");

        const int x = fr.at("x").get<int>();
        const int y = fr.at("y").get<int>();
        const int w = fr.at("w").get<int>();
        const int h = fr.at("h").get<int>();

        const float durationSeconds =
            item.value("duration", 100.0f) / 1000.0f;

        outFrames.push_back(Frame{
            sf::IntRect({x, y}, {w, h}),
            durationSeconds
        });
    }

    if (outFrames.empty()) {
        throw std::runtime_error("No frames parsed from JSON: " + jsonPath.string());
    }
}

void ResourceManager::loadAnimationSet(CharacterAnimations& outAnimations,
                                       std::size_t animIndex,
                                       const std::filesystem::path& root,
                                       const std::string& baseName,
                                       bool looping)
{
    AnimSet set{};

    for (int dir = 1; dir <= 8; ++dir) {
        const auto folder = root / baseName;

        const auto pngPath =
            folder / ("Businessman_" + baseName + "_dir" + std::to_string(dir) + ".png");
        const auto jsonPath =
            folder / ("Businessman_" + baseName + "_dir" + std::to_string(dir) + ".json");

        Clip clip;
        clip.looping = looping;
        clip.texture = std::make_shared<sf::Texture>();

        if (!clip.texture->loadFromFile(pngPath)) {
            throw std::runtime_error("Failed to load texture: " + pngPath.string());
        }

        parseFramesFromJson(jsonPath, clip.frames);
        set.byFacing[static_cast<std::size_t>(dir - 1)] = std::move(clip);
    }

    set.loaded = true;
    outAnimations.anims[animIndex] = std::move(set);
}

bool ResourceManager::loadBusinessmanCharacter(const std::string& id,
                                               const std::filesystem::path& assetRoot)
{
    try {
        CharacterAnimations animations{};

        loadAnimationSet(animations, kIdle,        assetRoot, "Idle",        true);
        loadAnimationSet(animations, kWalk,        assetRoot, "Walk",        true);
        loadAnimationSet(animations, kRunning,     assetRoot, "Running",     true);
        loadAnimationSet(animations, kJump,        assetRoot, "Jump",        false);
        loadAnimationSet(animations, kRunningJump, assetRoot, "RunningJump", false);
        loadAnimationSet(animations, kRunningRoll, assetRoot, "RunningRoll", false);
        loadAnimationSet(animations, kFightIdle,   assetRoot, "FightIdle",   true);
        loadAnimationSet(animations, kBlock,       assetRoot, "Block",       true);
        loadAnimationSet(animations, kLeftJab,     assetRoot, "LeftJab",     false);
        loadAnimationSet(animations, kRightHook,   assetRoot, "RightHook",   false);
        loadAnimationSet(animations, kUppercut,    assetRoot, "Uppercut",    false);
        loadAnimationSet(animations, kCombo,       assetRoot, "Combo",       false);
        loadAnimationSet(animations, kDamaged,     assetRoot, "Damaged",     false);
        loadAnimationSet(animations, kDie,         assetRoot, "Die",         false);
        loadAnimationSet(animations, kPickUp,      assetRoot, "PickUp",      false);
        loadAnimationSet(animations, kPickUpLow,   assetRoot, "PickUpLow",   false);
        loadAnimationSet(animations, kCarrying,    assetRoot, "Carrying",    true);
        loadAnimationSet(animations, kPull,        assetRoot, "Pull",        false);
        loadAnimationSet(animations, kPush,        assetRoot, "Push",        false);
        loadAnimationSet(animations, kTalking,     assetRoot, "Talking",     true);

        m_characterAnimations[id] = std::move(animations);
        return true;
    }
    catch (const std::exception& e) {
        m_logger.log_error("Failed to load character animations for id '",
                           id,
                           "' from ",
                           assetRoot,
                           ": ",
                           e.what());
        return false;
    }
}

const sf::Font& ResourceManager::getFont(const std::string& id) const
{
    return m_fonts.at(id);
}

sf::Font& ResourceManager::getFont(const std::string& id)
{
    return m_fonts.at(id);
}

const sf::Shader& ResourceManager::getShader(const std::string& id) const
{
    return m_shaders.at(id);
}

sf::Shader& ResourceManager::getShader(const std::string& id)
{
    return m_shaders.at(id);
}

const ResourceManager::CharacterAnimations&
ResourceManager::getCharacterAnimations(const std::string& id) const
{
    return m_characterAnimations.at(id);
}

ResourceManager::CharacterAnimations&
ResourceManager::getCharacterAnimations(const std::string& id)
{
    return m_characterAnimations.at(id);
}

bool ResourceManager::hasFont(const std::string& id) const
{
    return m_fonts.find(id) != m_fonts.end();
}

bool ResourceManager::hasShader(const std::string& id) const
{
    return m_shaders.find(id) != m_shaders.end();
}

bool ResourceManager::hasCharacterAnimations(const std::string& id) const
{
    return m_characterAnimations.find(id) != m_characterAnimations.end();
}
