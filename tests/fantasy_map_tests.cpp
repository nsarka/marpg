#include "client/rendering/map_layer.hpp"
#include "common/team_spawns.hpp"
#include "client/wall_occlusion.hpp"
#include <filesystem>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    check(argc>=2,"Root required");const bool gallery=argc>2;const auto path=(std::filesystem::path(argv[1])/(gallery?"assets/tiled/demo.tmx":"assets/tiled/arena.tmx")).string();
    tmx::Map map;check(map.load(path),"Fantasy map missing");
    for(const auto& tileset:map.getTilesets()) {
        if(tileset.getName()=="Gallery floor" || tileset.getName()=="Fantasy pack gallery" || tileset.getName()=="Fantasy") {
            check(static_cast<std::int32_t>(tileset.getTileOffset().x)==-64 && tileset.getTileOffset().y==14,"Fantasy authored pivot offset incorrect");
            check(common::tileAlignmentCorrection(tileset,256,128)==64.f,"Tiled alignment correction missing");
            // Pivot y=256*(1-.18) must land at the map cell center, within pixel rounding.
            check(std::abs(64.f-256.f+14.f+256.f*(1.f-.18f)-32.f)<.5f,"Fantasy pivot misses ground center");
        }
    }
    if(gallery) {
        unsigned characterSets=0;
        for(const auto& set:map.getTilesets()) {
            if(set.getImagePath().find("/Characters/")==std::string::npos)continue;
            ++characterSets;
            for(unsigned frame=0;frame<set.getTileCount();++frame)
                check(common::isPlayerAnimation(set,set.getFirstGID()+frame),"Character frame lacks player_animation property");
        }
        check(characterSets==168,"Missing character animation tilesets");
        sf::Vector2f size,offset;
        common::applyCharacterTileLayout({128,128},{128,64},size,offset);
        check(size==sf::Vector2f{256,256},"Gallery character scale differs from live players");
        check(64.f-size.y+offset.y+size.y*common::CharacterFeetY==32.f,"Gallery character feet not at cell center");
    }
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
    std::size_t floorIndex=0,wallIndex=0;
    for(std::size_t i=0;i<map.getLayers().size();++i) {
        if(map.getLayers()[i]->getName()=="Floor")floorIndex=i;
        if(map.getLayers()[i]->getName()=="Walls")wallIndex=i;
    }
    MapLayer floor(map,floorIndex),objects(map,wallIndex);WallOcclusion masks(map,wallIndex);
    sf::RenderTexture target({1100,650});target.setView(sf::View(sf::FloatRect({-1550,-50},{3228,1750})));
    if(gallery)target.setView(sf::View(sf::FloatRect({-900,600},{2200,1300})));
    if(gallery) {
        const auto& tiles=map.getLayers()[wallIndex]->getLayerAs<tmx::TileLayer>().getTiles();
        bool tested=false;
        for(std::size_t i=0;i<tiles.size();++i) {
            const auto found=map.getAnimatedTiles().find(tiles[i].ID);
            if(found==map.getAnimatedTiles().end() || found->second.animation.frames.size()<2)continue;
            const auto& frames=found->second.animation.frames;
            int x=int(i%map.getTileCount().x),y=int(i/map.getTileCount().x);
            objects.update(sf::Time::Zero);
            check(objects.getTile(x,y).ID==frames[0].tileID,"Zero update changed first frame");
            objects.update(sf::milliseconds(frames[0].duration-1));
            check(objects.getTile(x,y).ID==frames[0].tileID,"Animation advanced too early");
            objects.update(sf::milliseconds(1));
            check(objects.getTile(x,y).ID==frames[1].tileID,"World animation did not advance at frame boundary");
            int cycle=0;for(const auto& frame:frames)cycle+=frame.duration;
            objects.update(sf::milliseconds(cycle*3));
            check(objects.getTile(x,y).ID==frames[1].tileID,"Animation loop lost elapsed time");
            check(objects.getTile(x,y).flipFlags==tiles[i].flipFlags,"Animation lost tile orientation");
            tested=true;break;
        }
        check(tested,"No animated exhibit tested");
    }
    objects.update(sf::milliseconds(100));
    target.clear(sf::Color(35,40,35));target.draw(floor);target.draw(objects);target.display();
    check(target.getTexture().copyToImage().saveToFile(std::filesystem::temp_directory_path()/(gallery?"marpg-gallery.png":"marpg-fantasy-map.png")),"Preview failed");
}
