#pragma once
#include <SFML/Graphics.hpp>
#include "common/settings.hpp"
#include <tmxlite/Map.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>

struct TileLighting {
    bool enabled=false;
    sf::Vector3f sun{0,0,800};
    float ambient=.10f,intensity=.12f;
    struct LocalLight {sf::Vector2f position;float radius;sf::Vector3f color;float strength;float height=80,directionality=.5f,falloffExponent=1;};
    static constexpr std::size_t MaxLights=96;
    std::vector<LocalLight> lights;
    void addLight(sf::Vector2f position,float radius,sf::Vector3f color,float strength,float height=80,float directionality=.5f,float falloffExponent=1) {
        if(radius<=0 || strength<=0)return;
        // Combine repeated lightning pulses and their persistent spot light.
        for(auto& light:lights)if(light.position==position && light.color==color && light.height==height && light.directionality==directionality && light.falloffExponent==falloffExponent) {
            light.radius=std::max(light.radius,radius);light.strength=std::max(light.strength,strength);return;
        }
        if(lights.size()<MaxLights)lights.push_back({position,radius,color,strength,height,directionality,falloffExponent});
    }
    void addLight(sf::Vector2f position,const common::LightSettings& settings,float attackRadius=0,float fade=1) {
        if(!settings.enabled)return;
        addLight(position,std::max(float(settings.radius),attackRadius*float(settings.radiusMultiplier)),
            {float(settings.red),float(settings.green),float(settings.blue)},float(settings.intensity)*fade,
            float(settings.height),float(settings.directionality),float(settings.falloffExponent));
    }
    const sf::Texture* shadowMap=nullptr;
    sf::Vector2f shadowOrigin{};
    struct HeightTile {sf::Vector2f origin;sf::Image image;unsigned flips;unsigned stair=0;};
    std::vector<HeightTile> heightTiles;
    const sf::Texture* stairMap=nullptr;
    unsigned stairCount=0;
    void buildHeightField(sf::Texture& texture,sf::Texture& stairs) {
        if(heightTiles.empty()){shadowMap=nullptr;return;}
        sf::Vector2f low=heightTiles.front().origin,high=low;
        for(const auto& tile:heightTiles) {
            low.x=std::min(low.x,tile.origin.x);low.y=std::min(low.y,tile.origin.y);
            high.x=std::max(high.x,tile.origin.x+tile.image.getSize().x);
            high.y=std::max(high.y,tile.origin.y+tile.image.getSize().y+510.f);
        }
        shadowOrigin=low-sf::Vector2f{8,8};
        const sf::Vector2u size{unsigned((high.x-low.x)/4)+5,unsigned((high.y-low.y)/4)+5};
        sf::Image field(size,sf::Color::Transparent);
        stairCount=unsigned(std::count_if(heightTiles.begin(),heightTiles.end(),[](const auto& t){return t.stair!=0;}));
        sf::Image geometry({std::max(1u,stairCount),2},sf::Color::Transparent);
        unsigned stairIndex=0;
        for(const auto& tile:heightTiles) {
            if(tile.stair) {
                const unsigned x=unsigned(std::round((tile.origin.x-shadowOrigin.x)/(size.x*4.f)*65535.f));
                const unsigned y=unsigned(std::round((tile.origin.y-shadowOrigin.y)/(size.y*4.f)*65535.f));
                geometry.setPixel({stairIndex,0},sf::Color(x>>8,x&255,y>>8,y&255));
                geometry.setPixel({stairIndex++,1},sf::Color(tile.stair,tile.flips,0,255));
                continue;
            }
            const auto dimensions=tile.image.getSize();
            for(unsigned y=0;y<dimensions.y;++y)for(unsigned x=0;x<dimensions.x;++x) {
                float u=(x+.5f)/dimensions.x,v=(y+.5f)/dimensions.y;
                if(tile.flips&8)u=1-u;if(tile.flips&4)v=1-v;if(tile.flips&2)std::swap(u,v);
                const auto h=tile.image.getPixel({std::min(dimensions.x-1,unsigned(u*dimensions.x)),std::min(dimensions.y-1,unsigned(v*dimensions.y))});
                if(h.a<128 || h.r==0)continue;
                const int gx=int((tile.origin.x+x-shadowOrigin.x)/4),gy=int((tile.origin.y+y+h.r*2.f-shadowOrigin.y)/4);
                // Surface samples reconstruct ground columns. A one-cell footprint
                // closes raster cracks without turning a doorway into a solid wall.
                if(gx<0 || gy<0 || gx>=int(size.x) || gy>=int(size.y))continue;
                auto old=field.getPixel({unsigned(gx),unsigned(gy)});
                field.setPixel({unsigned(gx),unsigned(gy)},sf::Color(old.a?std::min(old.r,h.g):h.g,std::max(old.g,h.r),0,255));
            }
        }
        if(!texture.loadFromImage(field))throw std::runtime_error("Cannot upload world height field");
        shadowMap=&texture;
        if(!stairs.loadFromImage(geometry))throw std::runtime_error("Cannot upload stair geometry");
        stairMap=&stairs;
    }
    void load(const tmx::Map& map) {
        enabled=false;
        for(const auto& layer:map.getLayers()) {
            if(layer->getType()!=tmx::Layer::Type::Object || layer->getName()!="Lighting")continue;
            for(const auto& object:layer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
                if(object.getName()!="Sun")continue;
                if(enabled)throw std::runtime_error("Lighting layer must contain only one Sun");
                if(object.getShape()!=tmx::Object::Shape::Point)throw std::runtime_error("Sun must be a Tiled point object");
                const auto p=object.getPosition();const auto offset=layer->getOffset();
                sun={(p.x-p.y)*map.getTileSize().x/(2.f*map.getTileSize().y)+offset.x,(p.x+p.y)*.5f+offset.y,800};
                ambient=.10f;intensity=.12f;
                for(const auto& property:object.getProperties()) {
                    if(property.getName()!="height" && property.getName()!="ambient" && property.getName()!="intensity")continue;
                    float value;
                    if(property.getType()==tmx::Property::Type::Float)value=property.getFloatValue();
                    else if(property.getType()==tmx::Property::Type::Int)value=float(property.getIntValue());
                    else throw std::runtime_error("Sun lighting properties must be numbers");
                    if(property.getName()=="height")sun.z=value;
                    if(property.getName()=="ambient")ambient=value;
                    if(property.getName()=="intensity")intensity=value;
                }
                if(!std::isfinite(sun.z) || sun.z<=0 || !std::isfinite(ambient) || ambient<0 || ambient>1 ||
                   !std::isfinite(intensity) || intensity<0 || intensity>4)throw std::runtime_error("Invalid Sun height, ambient, or intensity");
                enabled=true;
            }
        }
    }
};
inline TileLighting tileLighting;
