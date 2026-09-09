#include "resource_manager.hpp"


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

bool ResourceManager::loadFantasyCharacter(const std::string& id,const std::filesystem::path& assetRoot)
{
    try {
        CharacterAnimations animations{};
        std::unordered_map<std::string,std::shared_ptr<sf::Texture>> textures;
        const auto load=[&](std::size_t slot,const std::string& name,bool looping,float frameTime=.05f) {
            auto& texture=textures[name];
            if(!texture) {
                texture=std::make_shared<sf::Texture>();
                if(!texture->loadFromFile(assetRoot/(name+".png")))throw std::runtime_error("Cannot load "+name);
                if(texture->getSize()!=sf::Vector2u{1920,1024})throw std::runtime_error("Expected 15 by 8 grid of 128-pixel frames: "+name);
            }
            auto& set=animations.anims[slot];
            // Engine: SW,W,NW,N,NE,E,SE,S. Asset rows: E,SE,S,SW,W,NW,N,NE.
            constexpr int rows[8]={3,4,5,6,7,0,1,2};
            for(std::size_t facing=0;facing<8;++facing) {
                auto& clip=set.byFacing[facing];clip.texture=texture;clip.looping=looping;
                for(int frame=0;frame<15;++frame)clip.frames.push_back({{{frame*128,rows[facing]*128},{128,128}},frameTime});
            }
            set.loaded=true;
        };
        load(kIdle,"Idle",true);load(kWalk,"Walk",true);load(kRunning,"Run",true);
        load(kJump,"Special1",false);load(kRunningJump,"Special1",false);load(kRunningRoll,"CrouchRun",false);
        load(kFightIdle,"Idle2",true);load(kBlock,"CrouchIdle",true);
        load(kLeftJab,"Attack1",false);load(kRightHook,"Attack4",false);
        load(kUppercut,"Special1",false);load(kCombo,"Attack3",false);
        load(kDamaged,"TakeDamage",false,.025f);load(kDie,"Die",false,.065f);
        load(kPickUp,"Special1",false);load(kPickUpLow,"Special1",false);
        load(kCarrying,"Special1",true);load(kPull,"Walk",false);load(kPush,"Walk",false);load(kTalking,"Taunt",true);
        m_characterAnimations[id]=std::move(animations);
        return true;
    } catch(const std::exception& e) {
        m_logger.log_error("Failed to load fantasy player: ",e.what());return false;
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
