#pragma once
#include <tmxlite/Map.hpp>
#include <stdexcept>
namespace common {
class LoadedMap {
    tmx::Map map_;
public:
    explicit LoadedMap(const std::string& path) {
        if(!map_.load(path))throw std::runtime_error("Cannot load map: "+path);
    }
    const tmx::Map& data() const {return map_;}
    std::size_t tileLayer(const std::string& name) const {
        for(std::size_t i=0;i<map_.getLayers().size();++i)
            if(map_.getLayers()[i]->getType()==tmx::Layer::Type::Tile && map_.getLayers()[i]->getName()==name)return i;
        throw std::runtime_error("Missing tile layer: "+name);
    }
};
}
