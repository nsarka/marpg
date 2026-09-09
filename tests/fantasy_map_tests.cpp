#include "common/map_layer.hpp"
#include "common/team_spawns.hpp"
#include "client/wall_occlusion.hpp"
#include <filesystem>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc>=2,"Root required");const bool gallery=argc>2;const auto path=(std::filesystem::path(argv[1])/(gallery?"assets/tiled/demo.tmx":"assets/tiled/arena.tmx")).string();
    tmx::Map map;check(map.load(path),"Fantasy map missing");
    common::CollisionWorld walls;walls.load(path);if(!gallery)check(walls.size()>20,"Fantasy collision missing");
    common::TriggerSystem triggers;triggers.load(path);
    common::TeamSpawns spawns;spawns.load(path,2,walls,triggers);
    if(!gallery)for(int y:{2,3,20,21})for(int x=(y<4?2:17);x<(y<4?7:22);++x) {
        sf::Vector2f point{float((x-y)*64+64),float((x+y)*32+32)};
        check(triggers.hasFloor(point) && !walls.overlaps(point),"Unsafe fantasy spawn");
    }
    if(gallery)for(int i=0;i<20;++i) {
        int x=4+i%10*2,y=5+i/10*2;
        sf::Vector2f p{float((x-y)*64+64),float((x+y)*32+32)};
        check(triggers.hasFloor(p) && !walls.overlaps(p),"Unsafe gallery spawn");
    }
    MapLayer floor(map,1),objects(map,2);WallOcclusion masks(map,2);
    sf::RenderTexture target({1100,650});target.setView(sf::View(sf::FloatRect({-1550,-50},{3228,1750})));
    if(gallery)target.setView(sf::View(sf::FloatRect({-900,600},{2200,1300})));
    objects.update(sf::milliseconds(100));
    target.clear(sf::Color(35,40,35));target.draw(floor);target.draw(objects);target.display();
    check(target.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/(gallery?"marpg-gallery.png":"marpg-fantasy-map.png")),"Preview failed");
}
