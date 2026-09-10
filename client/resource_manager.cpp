#include "resource_manager.hpp"


#include <stdexcept>
#include <utility>

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

const ResourceManager::AnimSet& ResourceManager::CharacterAnimations::get(common::CharacterAnimation animation) const {
    const auto index=static_cast<std::size_t>(animation);
    auto& set=anims.at(index);
    if(set.loaded)return set;
    const auto& definition=common::CharacterAnimations.at(index);
    auto texture=std::make_shared<sf::Texture>();
    if(!texture->loadFromFile(directory/(std::string(definition.name)+".png")))throw std::runtime_error("Cannot load character animation: "+std::string(definition.name));
    if(texture->getSize()!=sf::Vector2u{1920,1024})throw std::runtime_error("Expected 15 by 8 grid of 128-pixel frames");
    constexpr int rows[8]={3,4,5,6,7,0,1,2};
    for(std::size_t facing=0;facing<8;++facing) {
        auto& clip=set.byFacing[facing];clip.texture=texture;clip.looping=definition.looping;
        for(int frame=0;frame<15;++frame)clip.frames.push_back({{{frame*128,rows[facing]*128},{128,128}},definition.frameSeconds});
    }
    set.loaded=true;return set;
}

bool ResourceManager::loadFantasyCharacter(const std::string& id,const std::filesystem::path& assetRoot) {
    try {
        CharacterAnimations animations;animations.directory=assetRoot;
        for(const auto& definition:common::CharacterAnimations)
            if(!std::filesystem::is_regular_file(assetRoot/(std::string(definition.name)+".png")))throw std::runtime_error("Missing animation: "+std::string(definition.name));
        // Prewarm gameplay clips; the rest of the pack is loaded on first use.
        using A=common::CharacterAnimation;
        for(auto animation:{A::Idle,A::Walk,A::Run,A::Attack1,A::Attack4,A::Special1,A::TakeDamage,A::Die})animations.get(animation);
        m_characterAnimations[id]=std::move(animations);return true;
    }catch(const std::exception& error){m_logger.log_error("Cannot load character: ",error.what());return false;}
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
