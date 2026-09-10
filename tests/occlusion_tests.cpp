#include <filesystem>
#include <fstream>
#include <chrono>
#include <SFML/Graphics.hpp>
#include "client/wall_occlusion.hpp"
#include <stdexcept>
#include <iostream>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==2,"Shader path required");
    sf::Shader shader;
    check(shader.loadFromFile(argv[1],sf::Shader::Type::Fragment),"Shader compilation failed");
    sf::RenderTexture mask({32,32}), output({32,32});
    mask.clear(sf::Color::Transparent);
    sf::RectangleShape wall({32,16});
    const int depth=32768+110;
    wall.setFillColor(sf::Color(depth/256,depth%256,0,255));
    mask.draw(wall); mask.display();
    sf::Texture white;
    check(white.loadFromImage(sf::Image({32,32},sf::Color::White)),"Texture failed");
    sf::Sprite player(white);
    shader.setUniform("texture",sf::Shader::CurrentTexture);
    shader.setUniform("wallDepth",mask.getTexture());
    shader.setUniform("renderSize",sf::Glsl::Vec2(32,32));
    shader.setUniform("footDepth",90.f);
    shader.setUniform("outlined",false);
    sf::RenderStates states; states.shader=&shader;
    output.clear(sf::Color(200,150,50)); output.draw(player,states); output.display();
    auto image=output.getTexture().copyToImage();
    check(image.getPixel({8,8}).r<110,"Hidden pixels should be dark silhouettes");
    check(image.getPixel({8,24}).r==255,"Uncovered pixels must remain normal (mask orientation)");
    shader.setUniform("footDepth",120.f);
    output.clear(); output.draw(player,states); output.display();
    image=output.getTexture().copyToImage();
    check(image.getPixel({8,8}).r==255,"Player in front of wall must remain normal");
    {
        const auto dir=std::filesystem::temp_directory_path()/("marpg-mask-flips-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        sf::Image tile({32,64},sf::Color::Transparent);
        for(unsigned y=16;y<32;++y)for(unsigned x=4;x<12;++x)tile.setPixel({x,y},sf::Color::White);
        check(tile.saveToFile(dir/"tile.png"),"Fixture tile save failed");
        for(unsigned flags=0;flags<8;++flags) {
            const auto gid=1u|((flags&1)?0x80000000u:0)|((flags&2)?0x40000000u:0)|((flags&4)?0x20000000u:0);
            std::ofstream file(dir/"map.tmx");
            file<<"<map version='1.10' orientation='isometric' width='1' height='1' tilewidth='32' tileheight='64'>"
                "<tileset firstgid='1' name='mask' tilewidth='32' tileheight='64' tilecount='1' columns='0'>"
                "<tile id='0'><image source='tile.png' width='32' height='64'/><objectgroup><object id='1' x='4' y='16' width='8' height='16'/></objectgroup></tile></tileset>"
                "<layer name='Walls' width='1' height='1'><data encoding='csv'>"<<gid<<"</data></layer></map>";
            file.close();
            tmx::Map fixture;check(fixture.load((dir/"map.tmx").string()),"Mask fixture failed");
            WallOcclusion flipped(fixture,0);flipped.update({32,64},sf::View(sf::FloatRect({0,0},{32,64})));
            const auto result=flipped.texture().copyToImage();
            float u=.25f,v=.375f;if(flags&4)std::swap(u,v);if(flags&1)u=1-u;if(flags&2)v=1-v;
            check(result.getPixel({unsigned(u*32),unsigned(v*64)}).a==255,"Flipped wall mask misses transformed pixels");
            check(result.getPixel({unsigned((1-u)*32),unsigned((1-v)*64)}).a==0,"Flipped wall mask filled transparent pixels");
        }
        std::filesystem::remove_all(dir);
    }
    std::cout<<"PASS: shader compilation, partial occlusion, mask orientation, foreground player\n";
}
