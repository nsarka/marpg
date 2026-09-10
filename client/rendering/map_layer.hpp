#pragma once
#include "map_chunk.hpp"
#include "tile_assets.hpp"
class MapLayer final : public sf::Drawable
{
public:
    MapLayer(const tmx::Map& map, std::size_t idx, TileLighting* lighting=nullptr)
        : ownedLighting_(lighting?nullptr:std::make_unique<TileLighting>(map)), tileLighting(lighting?*lighting:*ownedLighting_)
    {
        if(tileLighting.enabled) {
            m_lightingShader=std::make_shared<sf::Shader>();
            if(!m_lightingShader->loadFromFile("../shaders/tile_lighting.vert","../shaders/tile_lighting.frag"))
                throw std::runtime_error("Cannot load tile lighting shaders");
        }
        const auto& layers = map.getLayers();

        if (map.getOrientation() != tmx::Orientation::Isometric)
        {
            std::cout << "Map is not isometric - nothing will be drawn\n";
            return;
        }

        if (idx >= layers.size())
        {
            std::cout << "Layer index " << idx << " is out of range, layer count is " << layers.size() << '\n';
            return;
        }

        if (layers[idx]->getType() != tmx::Layer::Type::Tile)
        {
            std::cout << "Layer " << idx << " is not a Tile layer\n";
            return;
        }

        const auto tileSize = map.getTileSize();
        m_mapTileSize = {tileSize.x, tileSize.y};
        m_mapTileCount = {map.getTileCount().x, map.getTileCount().y};

        // Keep chunk size aligned to whole tiles
        m_chunkSize.x = std::floor(m_chunkSize.x / static_cast<float>(tileSize.x)) * static_cast<float>(tileSize.x);
        m_chunkSize.y = std::floor(m_chunkSize.y / static_cast<float>(tileSize.y)) * static_cast<float>(tileSize.y);

        const auto& layer = layers[idx]->getLayerAs<tmx::TileLayer>();
        m_assets.configure(m_mapTileSize,m_lightingShader,tileLighting);
        createChunks(map, layer);

        // Approximate screen-space bounds for an isometric diamond map.
        const float halfW = static_cast<float>(m_mapTileSize.x) * 0.5f;
        const float halfH = static_cast<float>(m_mapTileSize.y) * 0.5f;
        const float mapPixelWidth = static_cast<float>(m_mapTileCount.x + m_mapTileCount.y) * halfW;
        const float mapPixelHeight = static_cast<float>(m_mapTileCount.x + m_mapTileCount.y) * halfH;

        m_globalBounds.position = {
            -static_cast<float>(m_mapTileCount.y) * halfW,
            0.f
        };
        m_globalBounds.size = {
            mapPixelWidth,
            mapPixelHeight
        };
    }

    ~MapLayer() = default;
    MapLayer(const MapLayer&) = delete;
    MapLayer& operator=(const MapLayer&) = delete;

    const sf::FloatRect& getGlobalBounds() const
    {
        return m_globalBounds;
    }

    void setTile(std::int32_t tileX, std::int32_t tileY, tmx::TileLayer::Tile tile, bool refresh = true)
    {
        sf::Vector2u chunkLocale;
        auto& selectedChunk = getChunkAndTransform(tileX, tileY, chunkLocale);
        selectedChunk->setTile(chunkLocale.x, chunkLocale.y, tile, refresh);
    }

    tmx::TileLayer::Tile getTile(std::int32_t tileX, std::int32_t tileY)
    {
        sf::Vector2u chunkLocale;
        auto& selectedChunk = getChunkAndTransform(tileX, tileY, chunkLocale);
        return selectedChunk->getTile(chunkLocale.x, chunkLocale.y);
    }

    void setColor(std::int32_t tileX, std::int32_t tileY, sf::Color color, bool refresh = true)
    {
        sf::Vector2u chunkLocale;
        auto& selectedChunk = getChunkAndTransform(tileX, tileY, chunkLocale);
        selectedChunk->setColor(chunkLocale.x, chunkLocale.y, color, refresh);
    }

    sf::Color getColor(std::int32_t tileX, std::int32_t tileY)
    {
        sf::Vector2u chunkLocale;
        auto& selectedChunk = getChunkAndTransform(tileX, tileY, chunkLocale);
        return selectedChunk->getColor(chunkLocale.x, chunkLocale.y);
    }

    void update(sf::Time elapsed)
    {
        for(auto& chunk:m_chunks) {
            bool changed=false;
            for(auto& animation:chunk->getActiveAnimations()) {
                const auto& frames=animation.animTile.animation.frames;
                if(frames.empty())continue;
                std::int64_t cycle=0;
                for(const auto& frame:frames)cycle+=std::max<std::int64_t>(1,frame.duration)*1000;
                const auto time=(animation.currentTime.asMicroseconds()+std::max<std::int64_t>(0,elapsed.asMicroseconds()))%cycle;
                animation.currentTime=sf::microseconds(time);
                auto remaining=time;
                auto id=frames.front().tileID;
                for(const auto& frame:frames) {
                    const auto duration=std::max<std::int64_t>(1,frame.duration)*1000;
                    if(remaining<duration){id=frame.tileID;break;}
                    remaining-=duration;
                }
                const auto x=static_cast<std::int32_t>(animation.tileCoords.x),y=static_cast<std::int32_t>(animation.tileCoords.y);
                if(getTile(x,y).ID!=id) {
                    tmx::TileLayer::Tile tile;tile.ID=id;tile.flipFlags=animation.flipFlags;
                    setTile(x,y,tile,false);changed=true;
                }
            }
            // Multiple exhibits in a chunk advance together; rebuild only once.
            if(changed)chunk->refresh();
        }
    }

private:
    std::unique_ptr<TileLighting> ownedLighting_;
    TileLighting& tileLighting;
    using Chunk=map_detail::Chunk;
    sf::Vector2f m_chunkSize{512.f, 512.f}; // measured in map tile pixels for chunk partitioning
    sf::Vector2u m_chunkCount{0u, 0u};
    sf::Vector2u m_mapTileSize{0u, 0u};
    sf::Vector2u m_mapTileCount{0u, 0u};
    sf::FloatRect m_globalBounds;
    sf::Vector2f m_offset{0.f, 0.f};

    std::shared_ptr<sf::Shader> m_lightingShader;
    map_detail::TileAssets m_assets;

    std::vector<Chunk::Ptr> m_chunks;

    Chunk::Ptr& getChunkAndTransform(std::int32_t x, std::int32_t y, sf::Vector2u& chunkRelative)
    {
        const std::uint32_t tilesPerChunkX = static_cast<std::uint32_t>(m_chunkSize.x) / m_mapTileSize.x;
        const std::uint32_t tilesPerChunkY = static_cast<std::uint32_t>(m_chunkSize.y) / m_mapTileSize.y;

        const std::uint32_t chunkX = static_cast<std::uint32_t>(x) / tilesPerChunkX;
        const std::uint32_t chunkY = static_cast<std::uint32_t>(y) / tilesPerChunkY;

        chunkRelative.x = static_cast<std::uint32_t>(x) - chunkX * tilesPerChunkX;
        chunkRelative.y = static_cast<std::uint32_t>(y) - chunkY * tilesPerChunkY;

        return m_chunks[chunkX + chunkY * m_chunkCount.x];
    }

    void createChunks(const tmx::Map& map, const tmx::TileLayer& layer)
    {
        const auto& tileSets = map.getTilesets();
        const auto& layerTiles = layer.getTiles();

        std::uint32_t maxID = std::numeric_limits<std::uint32_t>::max();
        std::vector<const tmx::Tileset*> usedTileSets;

        for (auto i = tileSets.rbegin(); i != tileSets.rend(); ++i)
        {
            for (const auto& tile : layerTiles)
            {
                if (tile.ID >= i->getFirstGID() && tile.ID < maxID)
                {
                    usedTileSets.push_back(&(*i));
                    break;
                }
            }
            maxID = i->getFirstGID();
        }

        for (const auto* tileset : usedTileSets)
        {
            m_assets.registerTilesetVisuals(*tileset);
        }

        const auto bounds = map.getBounds();

        m_chunkCount.x = static_cast<std::uint32_t>(std::ceil(bounds.width / m_chunkSize.x));
        m_chunkCount.y = static_cast<std::uint32_t>(std::ceil(bounds.height / m_chunkSize.y));

        const std::uint32_t tilesPerChunkX = static_cast<std::uint32_t>(m_chunkSize.x) / m_mapTileSize.x;
        const std::uint32_t tilesPerChunkY = static_cast<std::uint32_t>(m_chunkSize.y) / m_mapTileSize.y;

        for (std::uint32_t chunkY = 0; chunkY < m_chunkCount.y; ++chunkY)
        {
            for (std::uint32_t chunkX = 0; chunkX < m_chunkCount.x; ++chunkX)
            {
                const sf::Vector2u startTile{
                    chunkX * tilesPerChunkX,
                    chunkY * tilesPerChunkY
                };

                sf::Vector2u tileCount{
                    tilesPerChunkX,
                    tilesPerChunkY
                };

                if (startTile.x + tileCount.x > m_mapTileCount.x)
                {
                    tileCount.x = m_mapTileCount.x - startTile.x;
                }

                if (startTile.y + tileCount.y > m_mapTileCount.y)
                {
                    tileCount.y = m_mapTileCount.y - startTile.y;
                }

                m_chunks.emplace_back(std::make_unique<Chunk>(
                    layer,
                    m_assets.visuals(),
                    startTile,
                    tileCount,
                    m_mapTileSize,
                    map.getTileCount().x,
                    map.getAnimatedTiles(),tileLighting));
            }
        }
    }

    void draw(sf::RenderTarget& rt, sf::RenderStates states) const override
    {
        // Until isometric culling accounts for overhanging sprites, draw every nonempty chunk.
        for (const auto& chunk : m_chunks)
            if (!chunk->empty()) rt.draw(*chunk, states);
    }
};
