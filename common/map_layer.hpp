#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>

#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <tmxlite/detail/Log.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class MapLayer final : public sf::Drawable
{
public:
    MapLayer(const tmx::Map& map, std::size_t idx)
    {
        const auto& layers = map.getLayers();

        std::cout << "Map has " << layers.size() << " layers\n";
        for (std::size_t i = 0; i < layers.size(); ++i)
        {
            std::cout << "Layer " << i
                    << " name=\"" << layers[i]->getName() << "\""
                    << " type=";

            switch (layers[i]->getType())
            {
            case tmx::Layer::Type::Tile:
                std::cout << "Tile";
                break;
            case tmx::Layer::Type::Object:
                std::cout << "Object";
                break;
            case tmx::Layer::Type::Image:
                std::cout << "Image";
                break;
            case tmx::Layer::Type::Group:
                std::cout << "Group";
                break;
            default:
                std::cout << "Other";
                break;
            }

            std::cout << '\n';
        }

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
        m_chunkSize.x = std::floor(m_chunkSize.x / tileSize.x) * tileSize.x;
        m_chunkSize.y = std::floor(m_chunkSize.y / tileSize.y) * tileSize.y;

        m_mapTileSize.x = tileSize.x;
        m_mapTileSize.y = tileSize.y;

        const auto& layer = layers[idx]->getLayerAs<tmx::TileLayer>();
        createChunks(map, layer);

        std::cout << "Registered tile visuals: " << m_tileVisuals.size() << "\n";

        const auto mapSize = map.getBounds();
        m_globalBounds.size = {mapSize.width, mapSize.height};
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

    void setOffset(sf::Vector2f offset)
    {
        m_offset = offset;
    }

    sf::Vector2f getOffset() const
    {
        return m_offset;
    }

    void update(sf::Time elapsed)
    {
        for (auto* chunk : m_visibleChunks)
        {
            for (AnimationState& as : chunk->getActiveAnimations())
            {
                as.currentTime += elapsed;

                tmx::TileLayer::Tile tile;
                std::int32_t animTime = 0;
                auto frameIt = as.animTile.animation.frames.begin();

                while (animTime < as.currentTime.asMilliseconds())
                {
                    if (frameIt == as.animTile.animation.frames.end())
                    {
                        frameIt = as.animTile.animation.frames.begin();
                        as.currentTime -= sf::milliseconds(animTime);
                        animTime = 0;
                    }

                    tile.ID = frameIt->tileID;
                    animTime += frameIt->duration;
                    ++frameIt;
                }

                tile.flipFlags = as.flipFlags;
                setTile(static_cast<std::int32_t>(as.tileCoords.x),
                        static_cast<std::int32_t>(as.tileCoords.y),
                        tile);
            }
        }
    }

private:
    using TextureResource = std::map<std::string, std::unique_ptr<sf::Texture>>;

    struct AnimationState
    {
        sf::Vector2u tileCoords;
        sf::Time startTime;
        sf::Time currentTime;
        tmx::Tileset::Tile animTile;
        std::uint8_t flipFlags = 0;
    };

    struct TileVisual
    {
        std::string textureKey;
        const sf::Texture* texture = nullptr;
        sf::Vector2f texTopLeft{0.f, 0.f};
        sf::Vector2f texSize{0.f, 0.f};
        sf::Vector2f drawSize{0.f, 0.f};
    };

    class Chunk final : public sf::Transformable, public sf::Drawable
    {
    public:
        using Ptr = std::unique_ptr<Chunk>;
        using Tile = std::array<sf::Vertex, 6u>;

        Chunk(const tmx::TileLayer& layer,
              const std::map<std::uint32_t, TileVisual>& tileVisuals,
              const sf::Vector2f& position,
              const sf::Vector2f& tileCount,
              const sf::Vector2u& tileSize,
              std::size_t rowSize,
              const std::map<std::uint32_t, tmx::Tileset::Tile>& animTiles)
            : m_tileVisuals(tileVisuals)
            , m_animTiles(animTiles)
        {
            setPosition(position);

            m_layerOpacity = static_cast<std::uint8_t>(layer.getOpacity() * 255.f);

            const auto offset = layer.getOffset();
            m_layerOffset = {static_cast<float>(offset.x), static_cast<float>(offset.y)};
            m_chunkTileCount = {tileCount.x, tileCount.y};
            m_mapTileSize = tileSize;

            const sf::Color vertColour{200, 200, 200, m_layerOpacity};
            const auto& tileIDs = layer.getTiles();

            const std::size_t xPos = static_cast<std::size_t>(position.x / tileSize.x);
            const std::size_t yPos = static_cast<std::size_t>(position.y / tileSize.y);

            for (std::size_t y = yPos; y < yPos + static_cast<std::size_t>(tileCount.y); ++y)
            {
                for (std::size_t x = xPos; x < xPos + static_cast<std::size_t>(tileCount.x); ++x)
                {
                    const auto idx = (y * rowSize + x);
                    m_chunkTileIDs.push_back(tileIDs[idx]);
                    m_chunkColors.push_back(vertColour);
                }
            }

            generateTiles(true);
        }

        ~Chunk() = default;
        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;

        std::vector<AnimationState>& getActiveAnimations()
        {
            return m_activeAnimations;
        }

        tmx::TileLayer::Tile getTile(std::int32_t x, std::int32_t y) const
        {
            return m_chunkTileIDs[calcIndexFrom(x, y)];
        }

        void setTile(std::int32_t x, std::int32_t y, tmx::TileLayer::Tile tile, bool refresh)
        {
            m_chunkTileIDs[calcIndexFrom(x, y)] = tile;
            maybeRegenerate(refresh);
        }

        sf::Color getColor(std::int32_t x, std::int32_t y) const
        {
            return m_chunkColors[calcIndexFrom(x, y)];
        }

        void setColor(std::int32_t x, std::int32_t y, sf::Color color, bool refresh)
        {
            m_chunkColors[calcIndexFrom(x, y)] = color;
            maybeRegenerate(refresh);
        }

        bool empty() const
        {
            for (const auto& [_, array] : m_chunkArrays)
            {
                if (!array->empty())
                {
                    return false;
                }
            }
            return true;
        }

    private:
        class ChunkArray final : public sf::Drawable
        {
        public:
            using Ptr = std::unique_ptr<ChunkArray>;

            explicit ChunkArray(const sf::Texture& texture)
                : m_texture(texture)
            {
            }

            void reset()
            {
                m_vertices.clear();
            }

            void addTile(const Tile& tile)
            {
                for (const auto& v : tile)
                {
                    m_vertices.push_back(v);
                }
            }

            bool empty() const
            {
                return m_vertices.empty();
            }

        private:
            const sf::Texture& m_texture;
            std::vector<sf::Vertex> m_vertices;

            void draw(sf::RenderTarget& rt, sf::RenderStates states) const override
            {
                if (m_vertices.empty())
                {
                    return;
                }

                states.texture = &m_texture;
                rt.draw(m_vertices.data(), m_vertices.size(), sf::PrimitiveType::Triangles, states);
            }
        };

        std::uint8_t m_layerOpacity = 255;
        sf::Vector2f m_layerOffset{0.f, 0.f};
        sf::Vector2u m_mapTileSize{0u, 0u};
        sf::Vector2f m_chunkTileCount{0.f, 0.f};

        std::vector<tmx::TileLayer::Tile> m_chunkTileIDs;
        std::vector<sf::Color> m_chunkColors;

        const std::map<std::uint32_t, TileVisual>& m_tileVisuals;
        std::map<std::uint32_t, tmx::Tileset::Tile> m_animTiles;
        std::vector<AnimationState> m_activeAnimations;
        std::map<std::string, ChunkArray::Ptr> m_chunkArrays;

        ChunkArray& getOrCreateChunkArray(const TileVisual& visual)
        {
            auto found = m_chunkArrays.find(visual.textureKey);
            if (found != m_chunkArrays.end())
            {
                return *found->second;
            }

            auto array = std::make_unique<ChunkArray>(*visual.texture);
            auto* raw = array.get();
            m_chunkArrays.emplace(visual.textureKey, std::move(array));
            return *raw;
        }

        void maybeRegenerate(bool refresh)
        {
            if (!refresh)
            {
                return;
            }

            for (auto& [_, array] : m_chunkArrays)
            {
                array->reset();
            }

            generateTiles();
        }

        std::int32_t calcIndexFrom(std::int32_t x, std::int32_t y) const
        {
            return x + y * static_cast<std::int32_t>(m_chunkTileCount.x);
        }

        static void flipY(sf::Vector2f* v0, sf::Vector2f* v1, sf::Vector2f* v2, sf::Vector2f* v3, sf::Vector2f* v4, sf::Vector2f* v5)
        {
            const sf::Vector2f tmp0 = *v0;
            v0->y = v5->y;
            v3->y = v5->y;
            v5->y = tmp0.y;

            const sf::Vector2f tmp2 = *v2;
            v2->y = v1->y;
            v4->y = v1->y;
            v1->y = tmp2.y;
        }

        static void flipX(sf::Vector2f* v0, sf::Vector2f* v1, sf::Vector2f* v2, sf::Vector2f* v3, sf::Vector2f* v4, sf::Vector2f* v5)
        {
            const sf::Vector2f tmp0 = *v0;
            v0->x = v1->x;
            v3->x = v1->x;
            v1->x = tmp0.x;

            const sf::Vector2f tmp2 = *v2;
            v2->x = v5->x;
            v4->x = v5->x;
            v5->x = tmp2.x;
        }

        static void flipD(sf::Vector2f* v0, sf::Vector2f* v1, sf::Vector2f* v2, sf::Vector2f* v3, sf::Vector2f* v4, sf::Vector2f* v5)
        {
            const sf::Vector2f tmp2 = *v2;
            *v2 = *v4;
            *v4 = tmp2;

            const sf::Vector2f tmp0 = *v0;
            *v0 = *v3;
            *v3 = tmp0;

            const sf::Vector2f tmp1 = *v1;
            *v1 = *v5;
            *v5 = tmp1;
        }

        static void doFlips(std::uint8_t bits,
                            sf::Vector2f* v0, sf::Vector2f* v1, sf::Vector2f* v2,
                            sf::Vector2f* v3, sf::Vector2f* v4, sf::Vector2f* v5)
        {
            if (!(bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                !(bits & tmx::TileLayer::FlipFlag::Vertical) &&
                !(bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                return;
            }
            else if (!(bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     (bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     !(bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipY(v0, v1, v2, v3, v4, v5);
            }
            else if ((bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     !(bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     !(bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipX(v0, v1, v2, v3, v4, v5);
            }
            else if ((bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     (bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     !(bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipY(v0, v1, v2, v3, v4, v5);
                flipX(v0, v1, v2, v3, v4, v5);
            }
            else if (!(bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     !(bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     (bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipD(v0, v1, v2, v3, v4, v5);
            }
            else if (!(bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     (bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     (bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipX(v0, v1, v2, v3, v4, v5);
                flipD(v0, v1, v2, v3, v4, v5);
            }
            else if ((bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     !(bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     (bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipY(v0, v1, v2, v3, v4, v5);
                flipD(v0, v1, v2, v3, v4, v5);
            }
            else if ((bits & tmx::TileLayer::FlipFlag::Horizontal) &&
                     (bits & tmx::TileLayer::FlipFlag::Vertical) &&
                     (bits & tmx::TileLayer::FlipFlag::Diagonal))
            {
                flipY(v0, v1, v2, v3, v4, v5);
                flipX(v0, v1, v2, v3, v4, v5);
                flipD(v0, v1, v2, v3, v4, v5);
            }
        }

        void generateTiles(bool registerAnimation = false)
        {
            if (registerAnimation)
            {
                m_activeAnimations.clear();
            }

            std::uint32_t idx = 0;
            const std::uint32_t xPos = static_cast<std::uint32_t>(getPosition().x / m_mapTileSize.x);
            const std::uint32_t yPos = static_cast<std::uint32_t>(getPosition().y / m_mapTileSize.y);

            for (std::uint32_t y = yPos; y < yPos + static_cast<std::uint32_t>(m_chunkTileCount.y); ++y)
            {
                for (std::uint32_t x = xPos; x < xPos + static_cast<std::uint32_t>(m_chunkTileCount.x); ++x)
                {
                    if (idx >= m_chunkTileIDs.size())
                    {
                        return;
                    }

                    const auto tile = m_chunkTileIDs[idx];
                    if (tile.ID == 0)
                    {
                        ++idx;
                        continue;
                    }

                    const auto visualIt = m_tileVisuals.find(tile.ID);
                    if (visualIt == m_tileVisuals.end() || visualIt->second.texture == nullptr)
                    {
                        ++idx;
                        continue;
                    }

                    const TileVisual& visual = visualIt->second;

                    if (registerAnimation)
                    {
                        const auto animIt = m_animTiles.find(tile.ID);
                        if (animIt != m_animTiles.end())
                        {
                            AnimationState as;
                            as.animTile = animIt->second;
                            as.startTime = sf::milliseconds(0);
                            as.currentTime = sf::milliseconds(0);
                            as.tileCoords = {x, y};
                            as.flipFlags = tile.flipFlags;
                            m_activeAnimations.push_back(as);
                        }
                    }

                    const sf::Vector2f worldTileOffset{
                        static_cast<float>(x) * static_cast<float>(m_mapTileSize.x),
                        static_cast<float>(y) * static_cast<float>(m_mapTileSize.y)
                            + static_cast<float>(m_mapTileSize.y)
                            - visual.drawSize.y
                    };

                    const sf::Vector2f localOffset = worldTileOffset - getPosition();

                    Tile quad =
                    {
                        sf::Vertex{localOffset, m_chunkColors[idx], visual.texTopLeft},
                        sf::Vertex{
                            localOffset + sf::Vector2f{visual.drawSize.x, 0.f},
                            m_chunkColors[idx],
                            visual.texTopLeft + sf::Vector2f{visual.texSize.x, 0.f}
                        },
                        sf::Vertex{
                            localOffset + visual.drawSize,
                            m_chunkColors[idx],
                            visual.texTopLeft + visual.texSize
                        },

                        sf::Vertex{localOffset, m_chunkColors[idx], visual.texTopLeft},
                        sf::Vertex{
                            localOffset + visual.drawSize,
                            m_chunkColors[idx],
                            visual.texTopLeft + visual.texSize
                        },
                        sf::Vertex{
                            localOffset + sf::Vector2f{0.f, visual.drawSize.y},
                            m_chunkColors[idx],
                            visual.texTopLeft + sf::Vector2f{0.f, visual.texSize.y}
                        }
                    };

                    doFlips(tile.flipFlags,
                            &quad[0].texCoords, &quad[1].texCoords, &quad[2].texCoords,
                            &quad[3].texCoords, &quad[4].texCoords, &quad[5].texCoords);

                    getOrCreateChunkArray(visual).addTile(quad);
                    ++idx;
                }
            }
        }

        void draw(sf::RenderTarget& rt, sf::RenderStates states) const override
        {
            states.transform *= getTransform();
            states.transform.translate(m_layerOffset);

            for (const auto& [_, array] : m_chunkArrays)
            {
                rt.draw(*array, states);
            }
        }
    };

    sf::Vector2f m_chunkSize{512.f, 512.f};
    sf::Vector2u m_chunkCount{0u, 0u};
    sf::Vector2u m_mapTileSize{0u, 0u};
    sf::FloatRect m_globalBounds;
    sf::Vector2f m_offset{0.f, 0.f};

    TextureResource m_textureResource;
    std::map<std::uint32_t, TileVisual> m_tileVisuals;

    std::vector<Chunk::Ptr> m_chunks;
    mutable std::vector<Chunk*> m_visibleChunks;

    static const tmx::Tileset* findTilesetForGID(const std::vector<tmx::Tileset>& tilesets, std::uint32_t gid)
    {
        const tmx::Tileset* result = nullptr;
        for (const auto& tileset : tilesets)
        {
            if (gid >= tileset.getFirstGID() && gid <= tileset.getLastGID())
            {
                result = &tileset;
            }
        }
        return result;
    }

    sf::Texture& loadTextureOrFallback(const std::string& path, const tmx::Tileset* tileset)
    {
        auto found = m_textureResource.find(path);
        if (found != m_textureResource.end())
        {
            return *found->second;
        }

        auto texture = std::make_unique<sf::Texture>();
        sf::Image image;
        sf::Image fallback({2u, 2u}, sf::Color::Magenta);

        if (!image.loadFromFile(path))
        {
            if (!texture->loadFromImage(fallback))
            {
                throw std::runtime_error("Unable to load fallback image");
            }
        }
        else
        {
            if (tileset && tileset->hasTransparency())
            {
                const auto transparency = tileset->getTransparencyColour();
                image.createMaskFromColor({transparency.r, transparency.g, transparency.b, transparency.a});
            }

            if (!texture->loadFromImage(image))
            {
                throw std::runtime_error("Unable to load image: " + path);
            }
        }

        auto* raw = texture.get();
        m_textureResource.emplace(path, std::move(texture));
        return *raw;
    }

    void registerAtlasTileset(const tmx::Tileset& tileset)
    {
        const auto atlasPath = tileset.getImagePath();
        if (atlasPath.empty())
        {
            return;
        }

        sf::Texture& texture = loadTextureOrFallback(atlasPath, &tileset);
        const auto tileSize = tileset.getTileSize();
        if (tileSize.x == 0 || tileSize.y == 0)
        {
            return;
        }

        const std::uint32_t columns =
            tileset.getColumnCount() > 0
                ? tileset.getColumnCount()
                : (texture.getSize().x / tileSize.x);

        if (columns == 0)
        {
            return;
        }

        const std::uint32_t spacing = tileset.getSpacing();
        const std::uint32_t margin = tileset.getMargin();

        for (std::uint32_t localID = 0; localID < tileset.getTileCount(); ++localID)
        {
            const std::uint32_t gid = tileset.getFirstGID() + localID;
            const std::uint32_t col = localID % columns;
            const std::uint32_t row = localID / columns;

            TileVisual visual;
            visual.textureKey = atlasPath;
            visual.texture = &texture;
            visual.texTopLeft = {
                static_cast<float>(margin + col * (tileSize.x + spacing)),
                static_cast<float>(margin + row * (tileSize.y + spacing))
            };
            visual.texSize = {
                static_cast<float>(tileSize.x),
                static_cast<float>(tileSize.y)
            };
            visual.drawSize = visual.texSize;

            m_tileVisuals[gid] = visual;
        }
    }

    void registerCollectionTileset(const tmx::Tileset& tileset)
    {
        for (const auto& tile : tileset.getTiles())
        {
            if (tile.imagePath.empty())
            {
                continue;
            }

            const std::uint32_t gid = tileset.getFirstGID() + tile.ID;

            sf::Texture& texture = loadTextureOrFallback(tile.imagePath, &tileset);
            const auto textureSize = texture.getSize();

            const std::uint32_t width = (tile.imageSize.x != 0) ? tile.imageSize.x : textureSize.x;
            const std::uint32_t height = (tile.imageSize.y != 0) ? tile.imageSize.y : textureSize.y;

            TileVisual visual;
            visual.textureKey = tile.imagePath;
            visual.texture = &texture;
            visual.texTopLeft = {0.f, 0.f};
            visual.texSize = {
                static_cast<float>(width),
                static_cast<float>(height)
            };
            visual.drawSize = visual.texSize;

            m_tileVisuals[gid] = visual;
        }
    }

    void registerTilesetVisuals(const tmx::Tileset& tileset)
    {
        if (!tileset.getImagePath().empty())
        {
            registerAtlasTileset(tileset);
        }
        else
        {
            registerCollectionTileset(tileset);
        }
    }

    Chunk::Ptr& getChunkAndTransform(std::int32_t x, std::int32_t y, sf::Vector2u& chunkRelative)
    {
        const std::uint32_t chunkX = (x * m_mapTileSize.x) / static_cast<std::uint32_t>(m_chunkSize.x);
        const std::uint32_t chunkY = (y * m_mapTileSize.y) / static_cast<std::uint32_t>(m_chunkSize.y);

        chunkRelative.x =
            ((x * m_mapTileSize.x) - chunkX * static_cast<std::uint32_t>(m_chunkSize.x)) / m_mapTileSize.x;
        chunkRelative.y =
            ((y * m_mapTileSize.y) - chunkY * static_cast<std::uint32_t>(m_chunkSize.y)) / m_mapTileSize.y;

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
            registerTilesetVisuals(*tileset);
        }

        for (const auto& [gid, _] : map.getAnimatedTiles())
        {
            const auto* tileset = findTilesetForGID(tileSets, gid);
            if (tileset)
            {
                registerTilesetVisuals(*tileset);
            }
        }

        const auto bounds = map.getBounds();
        m_chunkCount.x = static_cast<std::uint32_t>(std::ceil(bounds.width / m_chunkSize.x));
        m_chunkCount.y = static_cast<std::uint32_t>(std::ceil(bounds.height / m_chunkSize.y));

        const sf::Vector2u tileSize{map.getTileSize().x, map.getTileSize().y};

        for (std::uint32_t y = 0; y < m_chunkCount.y; ++y)
        {
            sf::Vector2f tileCount{
                m_chunkSize.x / static_cast<float>(tileSize.x),
                m_chunkSize.y / static_cast<float>(tileSize.y)
            };

            for (std::uint32_t x = 0; x < m_chunkCount.x; ++x)
            {
                if ((x + 1u) * m_chunkSize.x > bounds.width)
                {
                    tileCount.x = (bounds.width - x * m_chunkSize.x) / static_cast<float>(map.getTileSize().x);
                }

                if ((y + 1u) * m_chunkSize.y > bounds.height)
                {
                    tileCount.y = (bounds.height - y * m_chunkSize.y) / static_cast<float>(map.getTileSize().y);
                }

                m_chunks.emplace_back(std::make_unique<Chunk>(
                    layer,
                    m_tileVisuals,
                    sf::Vector2f{x * m_chunkSize.x, y * m_chunkSize.y},
                    tileCount,
                    tileSize,
                    map.getTileCount().x,
                    map.getAnimatedTiles()));
            }
        }
    }

    void updateVisibility(const sf::View& view) const
    {
        sf::Vector2f viewCorner = view.getCenter();
        viewCorner -= view.getSize() / 2.f;

        const std::int32_t posX = static_cast<std::int32_t>(std::floor(viewCorner.x / m_chunkSize.x));
        const std::int32_t posY = static_cast<std::int32_t>(std::floor(viewCorner.y / m_chunkSize.y));
        const std::int32_t posX2 = static_cast<std::int32_t>(std::ceil((viewCorner.x + view.getSize().x) / m_chunkSize.x));
        const std::int32_t posY2 = static_cast<std::int32_t>(std::ceil((viewCorner.y + view.getSize().y) / m_chunkSize.y));

        std::vector<Chunk*> visible;
        for (std::int32_t y = posY; y < posY2; ++y)
        {
            for (std::int32_t x = posX; x < posX2; ++x)
            {
                if (x < 0 || y < 0)
                {
                    continue;
                }

                const std::size_t idx = static_cast<std::size_t>(x + y * static_cast<std::int32_t>(m_chunkCount.x));
                if (idx < m_chunks.size() && !m_chunks[idx]->empty())
                {
                    visible.push_back(m_chunks[idx].get());
                }
            }
        }

        std::swap(m_visibleChunks, visible);
    }

    void draw(sf::RenderTarget& rt, sf::RenderStates states) const override
    {
        states.transform.translate(m_offset);

        //updateVisibility(rt.getView());
        for (const auto* chunk : m_visibleChunks)
        {
            rt.draw(*chunk, states);
        }
    }
};