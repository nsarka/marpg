#include <SFML/Graphics.hpp>
#include "client/wall_occlusion.hpp"
#include <stdexcept>
#include <iostream>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    check(argc==3,"Shader and map paths required");
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
    tmx::Map map;
    check(map.load(argv[2]),"Demo map load failed");
    WallOcclusion walls(map,2);
    walls.update({1024,768},sf::View(sf::FloatRect({0,0},{1024,768})));
    auto realMask=walls.texture().copyToImage();
    auto wallPixel=realMask.getPixel({70,200});
    check(wallPixel.a==255,"Actual wall pixels missing from mask");
    check(int(wallPixel.r)*256+wallPixel.g-32768>200,"Wall depth must refer to its ground base");
    check(realMask.getPixel({464,444}).a==0,"Doorway opening must not mask the player");
    std::cout<<"PASS: shader compilation, partial occlusion, mask orientation, foreground player\n";
}
