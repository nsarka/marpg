#include "common/map_layer.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc==2,"Project root required");const std::filesystem::path root=argv[1];
    tmx::Map map;check(map.load((root/"assets/tiled/Demo.tmx").string()),"Map load failed");
    MapLayer floor(map,1),walls(map,2);
    check(tileLighting.enabled,"Sun point missing");
    check(std::isfinite(tileLighting.sun.x) && std::isfinite(tileLighting.sun.y) && tileLighting.sun.z>0,"Invalid Tiled Sun position");
    unsigned count=0;
    for(const auto& entry:std::filesystem::directory_iterator(root/"assets/tiled/Sprites")) {
        if(entry.path().extension()!=".png")continue;
        sf::Image source,albedo,normal,height;
        check(source.loadFromFile(entry.path()),"Source missing");
        const auto stem=entry.path().stem().string();
        check(albedo.loadFromFile(root/"assets/tiled/Lighting"/(stem+".albedo.png")) && normal.loadFromFile(root/"assets/tiled/Lighting"/(stem+".normal.png")),"Derived maps missing");
        check(height.loadFromFile(root/"assets/tiled/Lighting"/(stem+".height.png")),"Height map missing");
        check(height.getSize()==source.getSize(),"Height dimensions differ");
        check(source.getSize()==albedo.getSize() && source.getSize()==normal.getSize(),"Map dimensions differ");
        for(unsigned y=0;y<source.getSize().y;++y)for(unsigned x=0;x<source.getSize().x;++x) {
            auto a=source.getPixel({x,y}),b=albedo.getPixel({x,y}),n=normal.getPixel({x,y});
            check(a.a==b.a && a.a==n.a && a.a==height.getPixel({x,y}).a,"Alpha silhouette changed");
        }
        ++count;
    }
    check(count==11,"Missing tile maps");
    sf::Image fenceHeight;
    check(fenceHeight.loadFromFile(root/"assets/tiled/Lighting/fence_W.height.png"),"Fence height missing");
    check(fenceHeight.getPixel({128,466}).r==20 && fenceHeight.getPixel({248,406}).r==20,"Fence post heights disagree");
    check(fenceHeight.getPixel({180,454}).r==16 && fenceHeight.getPixel({220,434}).r==16,"Upper rail changes height along fence");
    check(fenceHeight.getPixel({180,464}).r==11 && fenceHeight.getPixel({220,444}).r==11,"Lower rail changes height along fence");
    check(fenceHeight.getPixel({180,454}).g==12 && fenceHeight.getPixel({180,464}).g==7,"Fence rails extend to the ground");

    for(const auto* stem:{"stairs_E","stairs_S"}) {
        sf::Image n;check(n.loadFromFile(root/"assets/tiled/Lighting"/(std::string(stem)+".normal.png")),"Stair normals missing");
        auto tread=n.getPixel({128,374}),shadowedTread=n.getPixel({128,362}),riser=n.getPixel({128,350});
        check(tread==shadowedTread,"Baked tread shadow changes surface normal");
        check(tread.g<64 && riser.g>128,"Stair tread and riser normals incorrect");
        check(std::string(stem)=="stairs_E"?riser.r>200:riser.r<64,"Stair riser points the wrong way");
    }
    // A northwest light must favor the screen-left wall face.
    sf::Image east,west;
    check(east.loadFromFile(root/"assets/tiled/Lighting/stairs_E.normal.png") &&
          west.loadFromFile(root/"assets/tiled/Lighting/stairs_S.normal.png"),"Stair maps missing");
    auto illumination=[](sf::Color c) {
        return -(c.r/127.5f-1.f)*1500.f-(c.g/127.5f-1.f)*600.f+(c.b/127.5f-1.f)*800.f;
    };
    check(illumination(west.getPixel({128,350}))>illumination(east.getPixel({128,350})),"Northwest light favors the east face");

    sf::Texture worldHeights,stairHeights;tileLighting.buildHeightField(worldHeights,stairHeights);
    check(tileLighting.shadowMap && !tileLighting.heightTiles.empty(),"World height field missing");
    sf::RenderTexture target({1000,800});target.setView(sf::View(sf::FloatRect({-1200,0},{2400,1920})));
    floor.update(sf::Time::Zero);walls.update(sf::Time::Zero);
    target.clear(sf::Color(30,34,42));target.draw(floor);target.draw(walls);target.display();
    check(target.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-light-current.png"),"Current lighting preview failed");
    tileLighting.sun={-1500,-600,800};
    target.clear(sf::Color(30,34,42));target.draw(floor);target.draw(walls);target.display();
    auto left=target.getTexture().copyToImage();check(left.saveToFile(std::filesystem::temp_directory_path()/"marpg-light-left.png"),"Preview failed");
    tileLighting.sun={1500,-600,800};
    target.clear(sf::Color(30,34,42));target.draw(floor);target.draw(walls);target.display();
    auto right=target.getTexture().copyToImage();check(right.saveToFile(std::filesystem::temp_directory_path()/"marpg-light-right.png"),"Preview failed");
    unsigned changed=0;
    for(unsigned y=0;y<800;++y)for(unsigned x=0;x<1000;++x)if(left.getPixel({x,y})!=right.getPixel({x,y}))++changed;
    check(changed>10000,"Sun movement did not relight tiles");
    tileLighting.addLight({0,700},400,{1,.9f,.72f},.6f);
    target.clear(sf::Color(30,34,42));target.draw(floor);target.draw(walls);target.display();
    auto lit=target.getTexture().copyToImage();
    unsigned locallyChanged=0;
    for(unsigned y=0;y<800;++y)for(unsigned x=0;x<1000;++x) {
        const auto world=target.mapPixelToCoords({int(x),int(y)});
        const auto delta=world-sf::Vector2f{0,700};
        if(delta.length()>565.f)check(lit.getPixel({x,y})==right.getPixel({x,y}),"Local light leaks outside radius");
        else if(lit.getPixel({x,y})!=right.getPixel({x,y}))++locallyChanged;
    }
    check(locallyChanged>1000,"Local light did not illuminate nearby tiles");
    sf::Texture shadowAtlas;
    // A thin 120-high wall blocks a chest-height light, while a 20-high
    // step does not. Columns encode lower/upper bounds in two-pixel units.
    sf::Image field({200,300},sf::Color::Transparent);
    for(unsigned y=0;y<300;++y)for(unsigned x=120;x<125;++x)field.setPixel({x,y},sf::Color(0,60,0,255));
    check(shadowAtlas.loadFromImage(field),"Height field upload failed");
    tileLighting.shadowMap=&shadowAtlas;tileLighting.shadowOrigin={-400,100};
    target.clear(sf::Color(30,34,42));target.draw(floor);target.draw(walls);target.display();
    auto shadowed=target.getTexture().copyToImage();
    unsigned blocked=0;
    for(unsigned y=0;y<800;++y)for(unsigned x=0;x<1000;++x)
        if(shadowed.getPixel({x,y})!=lit.getPixel({x,y}))++blocked;
    check(blocked>100,"Elevated wall did not cast a shadow");
    // Isolated shader regression: compare an opaque wall, low step, and
    // overhead lintel with identical ground footprints.
    sf::Shader probeShader;
    check(probeShader.loadFromFile("../shaders/tile_lighting.vert","../shaders/tile_lighting.frag"),"Probe shader load failed");
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
    sf::Texture stairAlbedo,stairNormal,stairHeight,stairData;
    check(stairAlbedo.loadFromFile(root/"assets/tiled/Lighting/stairs_S.albedo.png") &&
          stairNormal.loadFromFile(root/"assets/tiled/Lighting/stairs_S.normal.png") &&
          stairHeight.loadFromFile(root/"assets/tiled/Lighting/stairs_S.height.png"),"Stair preview maps failed");
    sf::Image geometry({1,2},sf::Color::Transparent);geometry.setPixel({0,1},sf::Color(2,0,0,255));
    check(stairData.loadFromImage(geometry),"Stair geometry failed");
    check(shadowAtlas.loadFromImage(sf::Image({256,256},sf::Color::Transparent)),"Empty field failed");
    probeShader.setUniform("receiverStair",2);
    probeShader.setUniform("stairCount",1);probeShader.setUniform("stairMap",stairData);
    probeShader.setUniform("shadowMap",shadowAtlas);probeShader.setUniform("shadowOrigin",sf::Glsl::Vec2(0,0));probeShader.setUniform("shadowSize",sf::Glsl::Vec2(1024,1024));
    probeShader.setUniform("normalMap",stairNormal);probeShader.setUniform("heightMap",stairHeight);
    probeShader.setUniform("ambient",.1f);
    const sf::Glsl::Vec4 stairLight{20,370,400,80};probeShader.setUniformArray("localLights",&stairLight,1);
    sf::RenderTexture stairPreview({512,512});stairPreview.setView(sf::View(sf::FloatRect({0,256},{256,256})));
    sf::Sprite stairSprite(stairAlbedo);sf::RenderStates stairStates;stairStates.shader=&probeShader;
    stairPreview.clear(sf::Color(45,45,45));stairPreview.draw(stairSprite,stairStates);stairPreview.display();
    check(stairPreview.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/"marpg-stair-shadow.png"),"Stair preview failed");
    tileLighting.shadowMap=nullptr;
    tileLighting.lights.clear();

    std::cout<<"PASS: Sun point, eleven matching texture pairs, shader rendering and moving light ("<<changed<<" changed pixels)\n";
}
