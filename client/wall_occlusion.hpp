#pragma once
#include <SFML/Graphics.hpp>
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

// An alpha mask stores the ground depth beneath each visible wall column.
// This preserves doorway holes and permits partial character silhouettes.
class WallOcclusion {
public:
    explicit WallOcclusion(const tmx::Map& map, std::size_t layerIndex) {
        const auto& layer = map.getLayers().at(layerIndex);
        const auto& tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();
        auto tileSize = map.getTileSize();
        const auto width = layer->getSize().x;
        const auto offset = layer->getOffset();
        for (std::size_t i=0; i<tiles.size(); ++i) {
            const tmx::Tileset::Tile* tile = nullptr;
            for (const auto& set : map.getTilesets()) {
                if (tiles[i].ID && set.hasTile(tiles[i].ID)) { tile=set.getTile(tiles[i].ID); break; }
            }
            // Walkable floor decorations do not hide players.
            if (!tile || tile->objectGroup.getObjects().empty()) continue;
            const bool hasSolid = std::any_of(tile->objectGroup.getObjects().begin(),
                tile->objectGroup.getObjects().end(), [](const auto& object) {
                    return object.getClass() != "DamageTrigger";
                });
            if (!hasSolid) continue;
            if (tiles[i].flipFlags) throw std::runtime_error("Flipped wall masks are not supported");
            sf::Image image;
            if (!image.loadFromFile(tile->imagePath)) throw std::runtime_error("Cannot load wall mask image");
            auto size=image.getSize();
            const float x=float(i%width), y=float(i/width);
            sf::Vector2f origin{(x-y)*tileSize.x*.5f+(float(tileSize.x)-size.x)*.5f+offset.x,
                                (x+y)*tileSize.y*.5f+float(tileSize.y)-size.y+offset.y};
            // The lower silhouette envelope bridges overhead doorway gaps.
            std::vector<sf::Vector2f> hull;
            for (unsigned col=0; col<size.x; ++col) {
                int bottom=-1;
                for (unsigned row=0; row<size.y; ++row)
                    if (image.getPixel({col,row}).a>25) bottom=int(row);
                if (bottom<0) continue;
                sf::Vector2f p{float(col),float(bottom)};
                while (hull.size()>1) {
                    auto a=hull[hull.size()-2], b=hull.back();
                    if ((b.x-a.x)*(p.y-b.y)-(b.y-a.y)*(p.x-b.x)<0) break;
                    hull.pop_back();
                }
                hull.push_back(p);
            }
            if (hull.size()<2) continue;
            sf::Image mask(size,sf::Color::Transparent);
            std::size_t segment=0;
            for (unsigned col=0; col<size.x; ++col) {
                while (segment+2<hull.size() && col>hull[segment+1].x) ++segment;
                auto a=hull[segment], b=hull[segment+1];
                float baseline=a.y+(float(col)-a.x)*(b.y-a.y)/(b.x-a.x);
                int depth=std::clamp(int(std::round(origin.y+baseline))+32768,0,65535);
                for (unsigned row=0; row<size.y; ++row) {
                    if (image.getPixel({col,row}).a>25)
                        mask.setPixel({col,row},sf::Color(depth/256,depth%256,0,255));
                }
            }
            auto texture=std::make_unique<sf::Texture>();
            if (!texture->loadFromImage(mask)) throw std::runtime_error("Cannot create wall depth mask");
            entries_.push_back({std::move(texture),origin,origin.y+hull.back().y});
        }
        std::stable_sort(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    }
    void update(sf::Vector2u size, const sf::View& view) {
        if (mask_.getSize()!=size && !mask_.resize(size)) throw std::runtime_error("Cannot resize wall depth target");
        mask_.clear(sf::Color::Transparent);
        mask_.setView(view);
        for (const auto& entry : entries_) {
            sf::Sprite sprite(*entry.texture); sprite.setPosition(entry.position);
            mask_.draw(sprite);
        }
        mask_.display();
    }
    const sf::Texture& texture() const {return mask_.getTexture();}
private:
    struct Entry {std::unique_ptr<sf::Texture> texture; sf::Vector2f position; float depth;};
    std::vector<Entry> entries_;
    sf::RenderTexture mask_;
};
