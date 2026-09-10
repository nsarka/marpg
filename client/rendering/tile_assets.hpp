#pragma once
#include "tile_visual.hpp"
namespace map_detail {
class TileAssets {
    TileLighting* lighting_=nullptr;
    using TextureResource=std::map<std::string,std::unique_ptr<sf::Texture>>;
    TextureResource m_textureResource;
    sf::Vector2u m_mapTileSize{};
    std::shared_ptr<sf::Shader> m_lightingShader;
    std::map<std::uint32_t,TileVisual> m_tileVisuals;
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
            visual.offset={float(static_cast<std::int32_t>(tileset.getTileOffset().x)),float(static_cast<std::int32_t>(tileset.getTileOffset().y))};
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
            visual.offset.x += common::tileAlignmentCorrection(tileset,visual.drawSize.x,float(m_mapTileSize.x));
            if(common::isPlayerAnimation(tileset,gid))
                common::applyCharacterTileLayout(visual.texSize,sf::Vector2f(m_mapTileSize),visual.drawSize,visual.offset);

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
            visual.offset={float(static_cast<std::int32_t>(tileset.getTileOffset().x)),float(static_cast<std::int32_t>(tileset.getTileOffset().y))};
            visual.textureKey = tile.imagePath;
            visual.texture = &texture;
            const std::filesystem::path source(tile.imagePath);
            const auto folder=source.parent_path().parent_path()/"Lighting";
            const auto stem=source.stem().string();
            visual.stair=stem=="stairs_E"?1:stem=="stairs_S"?2:stem=="stairsCornerOuter_S"?3:0;
            const auto albedo=folder/(source.stem().string()+".albedo.png");
            const auto normal=folder/(source.stem().string()+".normal.png");
            if(lighting_->enabled && std::filesystem::exists(albedo) && std::filesystem::exists(normal)) {
                visual.texture=&loadTextureOrFallback(albedo.string(),nullptr);
                visual.normal=&loadTextureOrFallback(normal.string(),nullptr);
                const auto height=folder/(source.stem().string()+".height.png");
                if(std::filesystem::exists(height))visual.height=&loadTextureOrFallback(height.string(),nullptr);
                visual.lightingShader=m_lightingShader;
            }
            visual.texTopLeft = {0.f, 0.f};
            visual.texSize = {
                static_cast<float>(width),
                static_cast<float>(height)
            };
            visual.drawSize = visual.texSize;
            visual.offset.x += common::tileAlignmentCorrection(tileset,visual.drawSize.x,float(m_mapTileSize.x));
            if(common::isPlayerAnimation(tileset,gid))
                common::applyCharacterTileLayout(visual.texSize,sf::Vector2f(m_mapTileSize),visual.drawSize,visual.offset);

            m_tileVisuals[gid] = visual;
        }
    }

public:
    void configure(sf::Vector2u size,std::shared_ptr<sf::Shader> shader,TileLighting& lighting){lighting_=&lighting;m_mapTileSize=size;m_lightingShader=std::move(shader);}
    const auto& visuals() const{return m_tileVisuals;}
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

};
}
