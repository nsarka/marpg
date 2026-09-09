#include "client/resource_manager.hpp"
#include "client/player.hpp"
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc==2,"Project root required");
    common::Logger logger;ResourceManager resources(logger);
    const auto root=std::filesystem::path(argv[1]);
    check(resources.loadFantasyCharacter("player",root/"assets/Fantasy tileset - 2D Isometric/Characters/Player"),"Fantasy player failed to load");
    const auto& animations=resources.getCharacterAnimations("player");
    constexpr int rows[]={3,4,5,6,7,0,1,2};
    for(const auto& set:animations.anims) {
        check(set.loaded,"Missing animation state");
        for(unsigned facing=0;facing<8;++facing) {
            const auto& clip=set.byFacing[facing];
            check(clip.texture && clip.frames.size()==15,"Missing animation frames");
            check(clip.frames.front().rect.position.y==rows[facing]*128,"Facing row incorrect");
            check(clip.frames.back().rect.position.x==14*128,"Last frame missing");
            for(const auto& frame:clip.frames)check(frame.durationSeconds>0 && frame.rect.size==sf::Vector2i{128,128},"Invalid frame");
        }
    }
    check(animations.anims[10].byFacing[0].texture==animations.anims[16].byFacing[0].texture,"Spells do not share Special1");
    check(!animations.anims[13].byFacing[0].looping && !animations.anims[12].byFacing[0].looping,"Death or hurt loops");
    check(animations.anims[8].byFacing[0].texture!=animations.anims[9].byFacing[0].texture,"Melee swings share a sheet");
    sf::RenderTexture preview({1024,256});preview.clear(sf::Color(45,45,45));
    for(unsigned facing=0;facing<8;++facing) {
        const auto& clip=animations.anims[0].byFacing[facing];
        sf::Sprite sprite(*clip.texture,clip.frames[0].rect);sprite.setScale({2,2});sprite.setOrigin({64,88});sprite.setPosition({64.f+128.f*facing,180});preview.draw(sprite);
    }
    preview.display();check(preview.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-fantasy-player.png"),"Preview save failed");
}
