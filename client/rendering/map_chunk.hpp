#pragma once
#include "common/map_geometry.hpp"
#include "tile_visual.hpp"
namespace map_detail {
    class Chunk final : public sf::Drawable
    {
    public:
        using Ptr = std::unique_ptr<Chunk>;
        using Tile = std::array<sf::Vertex, 6u>;

        Chunk(const tmx::TileLayer& layer,
              const std::map<std::uint32_t, TileVisual>& tileVisuals,
              const sf::Vector2u& startTile,
              const sf::Vector2u& tileCount,
              const sf::Vector2u& tileSize,
              std::size_t rowSize,
              const std::map<std::uint32_t, tmx::Tileset::Tile>& animTiles, TileLighting& lighting)
            : tileLighting(lighting), m_tileVisuals(tileVisuals)
            , m_animTiles(animTiles)
            , m_startTile(startTile)
            , m_chunkTileCount(tileCount)
            , m_mapTileSize(tileSize)
            , m_chunkScreenOrigin(tileToScreen(startTile.x, startTile.y, tileSize))
        {
            m_layerOpacity = static_cast<std::uint8_t>(layer.getOpacity() * 255.f);

            const auto offset = layer.getOffset();
            m_layerOffset = {static_cast<float>(offset.x), static_cast<float>(offset.y)};

            const sf::Color vertColour{200, 200, 200, m_layerOpacity};
            const auto& tileIDs = layer.getTiles();

            for (std::uint32_t localY = 0; localY < tileCount.y; ++localY)
            {
                for (std::uint32_t localX = 0; localX < tileCount.x; ++localX)
                {
                    const std::size_t mapX = static_cast<std::size_t>(startTile.x + localX);
                    const std::size_t mapY = static_cast<std::size_t>(startTile.y + localY);
                    const std::size_t idx = mapY * rowSize + mapX;

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

        void refresh(){maybeRegenerate(true);}

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

            explicit ChunkArray(const sf::Texture& texture,const sf::Texture* normal,const sf::Texture* height,unsigned stair,std::uint8_t flips,std::shared_ptr<sf::Shader> shader, TileLighting& lighting)
                : tileLighting(lighting), m_texture(texture),m_normal(normal),m_height(height),m_stair(stair),m_flips(flips),m_shader(std::move(shader))
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
            TileLighting& tileLighting;
            const sf::Texture& m_texture;
            const sf::Texture* m_normal;
            const sf::Texture* m_height;
            unsigned m_stair;
            std::uint8_t m_flips;
            std::shared_ptr<sf::Shader> m_shader;
            std::vector<sf::Vertex> m_vertices;

            void draw(sf::RenderTarget& rt, sf::RenderStates states) const override
            {
                if (m_vertices.empty())
                {
                    return;
                }

                if(m_normal && tileLighting.enabled) {
                    auto& shader=*m_shader;
                    const auto local=states.transform.getInverse().transformPoint({tileLighting.sun.x,tileLighting.sun.y});
                    shader.setUniform("texture",sf::Shader::CurrentTexture);
                    shader.setUniform("normalMap",*m_normal);
                    shader.setUniform("hasHeight",m_height!=nullptr);
                    shader.setUniform("receiverStair",int(m_stair));
                    if(m_height)shader.setUniform("heightMap",*m_height);
                    shader.setUniform("sunPosition",sf::Glsl::Vec3(local.x,local.y,tileLighting.sun.z));
                    shader.setUniform("flip",sf::Glsl::Vec3((m_flips&tmx::TileLayer::Horizontal)?1.f:0.f,(m_flips&tmx::TileLayer::Vertical)?1.f:0.f,(m_flips&tmx::TileLayer::Diagonal)?1.f:0.f));
                    shader.setUniform("ambient",tileLighting.ambient);shader.setUniform("intensity",tileLighting.intensity);
                    std::array<sf::Glsl::Vec4,TileLighting::MaxLights> lightPositions,lightColors;
                    std::array<sf::Glsl::Vec2,TileLighting::MaxLights> lightShapes;
                    for(std::size_t i=0;i<tileLighting.lights.size();++i) {
                        const auto& light=tileLighting.lights[i];
                        const auto point=states.transform.getInverse().transformPoint(light.position);
                        lightPositions[i]={point.x,point.y,light.radius,light.height};
                        lightShapes[i]={light.directionality,light.falloffExponent};
                        lightColors[i]={light.color.x,light.color.y,light.color.z,light.strength};
                    }
                    shader.setUniform("hasShadows",tileLighting.shadowMap!=nullptr);
                    shader.setUniform("stairCount",int(tileLighting.stairMap?tileLighting.stairCount:0));
                    if(tileLighting.stairMap)shader.setUniform("stairMap",*tileLighting.stairMap);
                    if(tileLighting.shadowMap) {
                        shader.setUniform("shadowMap",*tileLighting.shadowMap);
                        const auto origin=states.transform.getInverse().transformPoint(tileLighting.shadowOrigin);
                        shader.setUniform("shadowOrigin",sf::Glsl::Vec2(origin));
                        const auto size=tileLighting.shadowMap->getSize();
                        shader.setUniform("shadowSize",sf::Glsl::Vec2(size.x*4.f,size.y*4.f));
                    }
                    shader.setUniform("lightCount",static_cast<int>(tileLighting.lights.size()));
                    if(!tileLighting.lights.empty()) {
                        shader.setUniformArray("localLights",lightPositions.data(),tileLighting.lights.size());
                        shader.setUniformArray("lightColors",lightColors.data(),tileLighting.lights.size());
                        shader.setUniformArray("lightShapes",lightShapes.data(),tileLighting.lights.size());
                    }
                    states.shader=&shader;
                }
                states.texture = &m_texture;
                rt.draw(m_vertices.data(), m_vertices.size(), sf::PrimitiveType::Triangles, states);
            }
        };

        static sf::Vector2f tileToScreen(std::uint32_t tileX, std::uint32_t tileY, const sf::Vector2u& tileSize)
        {
            return common::tileToWorld(float(tileX),float(tileY),sf::Vector2f(tileSize));
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

        ChunkArray& getOrCreateChunkArray(const TileVisual& visual,std::uint8_t flips)
        {
            const auto key=visual.textureKey+"#"+std::to_string(flips);
            auto found = m_chunkArrays.find(key);
            if (found != m_chunkArrays.end())
            {
                return *found->second;
            }

            auto array = std::make_unique<ChunkArray>(*visual.texture,visual.normal,visual.height,visual.stair,flips,visual.lightingShader,tileLighting);
            auto* raw = array.get();
            m_chunkArrays.emplace(key, std::move(array));
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

        void generateTiles(bool registerAnimation = false)
        {
            if (registerAnimation)
            {
                m_activeAnimations.clear();
            }

            std::uint32_t idx = 0;
            for (std::uint32_t localY = 0; localY < m_chunkTileCount.y; ++localY)
            {
                for (std::uint32_t localX = 0; localX < m_chunkTileCount.x; ++localX)
                {
                    if (idx >= m_chunkTileIDs.size())
                    {
                        return;
                    }

                    const std::uint32_t mapX = m_startTile.x + localX;
                    const std::uint32_t mapY = m_startTile.y + localY;

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
                            as.currentTime = sf::milliseconds(0);
                            as.tileCoords = {mapX, mapY};
                            as.flipFlags = tile.flipFlags;
                            m_activeAnimations.push_back(as);
                        }
                    }

                    const sf::Vector2f isoBase = tileToScreen(mapX, mapY, m_mapTileSize);

                    // Anchor tiles by bottom-center onto the isometric tile base.
                    const auto worldTopLeft=common::tileImagePosition(isoBase,sf::Vector2f(m_mapTileSize),visual.drawSize,visual.offset);

                    if(registerAnimation && visual.height)
                        tileLighting.heightTiles.push_back({worldTopLeft+m_layerOffset,visual.height->copyToImage(),tile.flipFlags,visual.stair});

                    const sf::Vector2f localTopLeft = worldTopLeft - m_chunkScreenOrigin;

                    Tile quad =
                    {
                        sf::Vertex{localTopLeft, m_chunkColors[idx], visual.texTopLeft},
                        sf::Vertex{
                            localTopLeft + sf::Vector2f{visual.drawSize.x, 0.f},
                            m_chunkColors[idx],
                            visual.texTopLeft + sf::Vector2f{visual.texSize.x, 0.f}
                        },
                        sf::Vertex{
                            localTopLeft + visual.drawSize,
                            m_chunkColors[idx],
                            visual.texTopLeft + visual.texSize
                        },

                        sf::Vertex{localTopLeft, m_chunkColors[idx], visual.texTopLeft},
                        sf::Vertex{
                            localTopLeft + visual.drawSize,
                            m_chunkColors[idx],
                            visual.texTopLeft + visual.texSize
                        },
                        sf::Vertex{
                            localTopLeft + sf::Vector2f{0.f, visual.drawSize.y},
                            m_chunkColors[idx],
                            visual.texTopLeft + sf::Vector2f{0.f, visual.texSize.y}
                        }
                    };

                    doFlips(tile.flipFlags,
                            &quad[0].texCoords, &quad[1].texCoords, &quad[2].texCoords,
                            &quad[3].texCoords, &quad[4].texCoords, &quad[5].texCoords);

                    getOrCreateChunkArray(visual,tile.flipFlags).addTile(quad);
                    ++idx;
                }
            }
        }

        void draw(sf::RenderTarget& rt, sf::RenderStates states) const override
        {
            states.transform.translate(m_chunkScreenOrigin + m_layerOffset);

            for (const auto& [_, array] : m_chunkArrays)
            {
                rt.draw(*array, states);
            }
        }

        std::uint8_t m_layerOpacity = 255;
        sf::Vector2f m_layerOffset{0.f, 0.f};

        std::map<std::string, ChunkArray::Ptr> m_chunkArrays;
        const std::map<std::uint32_t, TileVisual>& m_tileVisuals;

        std::vector<tmx::TileLayer::Tile> m_chunkTileIDs;
        std::vector<sf::Color> m_chunkColors;

        TileLighting& tileLighting;
        std::map<std::uint32_t, tmx::Tileset::Tile> m_animTiles;
        std::vector<AnimationState> m_activeAnimations;

        sf::Vector2u m_startTile{0u, 0u};
        sf::Vector2u m_chunkTileCount{0u, 0u};
        sf::Vector2u m_mapTileSize{0u, 0u};
        sf::Vector2f m_chunkScreenOrigin{0.f, 0.f};
    };

}
