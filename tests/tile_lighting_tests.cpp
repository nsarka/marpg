#include "client/rendering/map_layer.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc==2,"Project root required");const std::filesystem::path root=argv[1];
    sf::Texture shadowAtlas;
    // Isolated shader regression: compare an opaque wall, low step, and
    // overhead lintel with identical ground footprints.
    sf::Shader probeShader;
    check(probeShader.loadFromFile(root/"shaders/tile_lighting.vert",root/"shaders/tile_lighting.frag"),"Probe shader load failed");
    sf::Texture white,up;
    check(white.loadFromImage(sf::Image({1,1},sf::Color::White)),"White texture failed");
    check(up.loadFromImage(sf::Image({1,1},sf::Color(128,23,201))),"Normal texture failed");
    sf::RenderTexture probe({64,32});probe.setView(sf::View(sf::FloatRect({0,0},{400,200})));
    sf::RectangleShape plane({400,200});plane.setTexture(&white);
    probeShader.setUniform("texture",sf::Shader::CurrentTexture);probeShader.setUniform("normalMap",up);
    probeShader.setUniform("hasHeight",false);probeShader.setUniform("hasShadows",true);
    probeShader.setUniform("flip",sf::Glsl::Vec3(0,0,0));probeShader.setUniform("sunPosition",sf::Glsl::Vec3(0,0,800));
    probeShader.setUniform("ambient",0.f);probeShader.setUniform("intensity",0.f);
    probeShader.setUniform("shadowOrigin",sf::Glsl::Vec2(72,-28));probeShader.setUniform("shadowSize",sf::Glsl::Vec2(256,256));
    const sf::Glsl::Vec4 lightPosition{100,100,400,80},lightColor{1,1,1,1};
    const sf::Glsl::Vec2 shape{.5f,1};probeShader.setUniformArray("lightShapes",&shape,1);
    probeShader.setUniform("lightCount",1);probeShader.setUniformArray("localLights",&lightPosition,1);probeShader.setUniformArray("lightColors",&lightColor,1);
    const auto brightness=[&](unsigned bottom,unsigned top) {
        sf::Image columns({64,64},sf::Color::Transparent);
        for(unsigned y=0;y<64;++y)for(unsigned x=32;x<34;++x)columns.setPixel({x,y},sf::Color(bottom/2,top/2,0,255));
        check(shadowAtlas.loadFromImage(columns),"Probe field failed");probeShader.setUniform("shadowMap",shadowAtlas);
        sf::RenderStates states;states.shader=&probeShader;
        probe.clear();probe.draw(plane,states);probe.display();return probe.getTexture().copyToImage().getPixel({48,16}).r;
    };
    check(brightness(0,120)==0,"Tall wall leaks light");
    check(brightness(0,20)>20,"Low step incorrectly blocks elevated light");
    check(brightness(100,160)>20,"Doorway lintel blocks light below opening");
    // Even a broad exponent must fade almost to black before the radius.
    const sf::Glsl::Vec2 broadShape{0,.1f};
    const sf::Glsl::Vec4 edgeLight{100,100,100,80};
    probeShader.setUniform("hasShadows",false);
    probeShader.setUniformArray("lightShapes",&broadShape,1);probeShader.setUniformArray("localLights",&edgeLight,1);
    sf::RenderStates edgeStates;edgeStates.shader=&probeShader;
    probe.clear();probe.draw(plane,edgeStates);probe.display();
    const auto edgeImage=probe.getTexture().copyToImage();
    check(edgeImage.getPixel({16,16}).r>240 && edgeImage.getPixel({31,16}).r<8 && edgeImage.getPixel({32,16}).r==0,"Light radius has a hard cutoff");
    probeShader.setUniform("hasShadows",true);probeShader.setUniformArray("lightShapes",&shape,1);
    // A visible riser can lie just inside its quantized shadow column.
    // It must not shadow itself when the light is on its outward side.
    sf::Texture riserHeight;
    check(riserHeight.loadFromImage(sf::Image({1,1},sf::Color(40,0,0))),"Riser height failed");
    check(up.loadFromImage(sf::Image({1,1},sf::Color(231,179,179))),"Riser normal failed");
    probeShader.setUniform("normalMap",up);probeShader.setUniform("hasHeight",true);probeShader.setUniform("heightMap",riserHeight);
    const sf::Glsl::Vec4 frontLight{300,180,400,80};
    probeShader.setUniformArray("localLights",&frontLight,1);
    sf::Image riserColumns({64,64},sf::Color::Transparent);
    for(unsigned y=0;y<64;++y)for(unsigned x=32;x<34;++x)riserColumns.setPixel({x,y},sf::Color(0,60,0,255));
    check(shadowAtlas.loadFromImage(riserColumns),"Riser field failed");probeShader.setUniform("shadowMap",shadowAtlas);
    sf::RenderStates riserStates;riserStates.shader=&probeShader;
    probe.clear();probe.draw(plane,riserStates);probe.display();
    check(probe.getTexture().copyToImage().getPixel({32,16}).r>40,"Riser shadows itself inside its raster column");
    std::cout<<"PASS: shader rendering, height-based shadows, low steps, lintels, falloff and self-shadow bias\n";
}
