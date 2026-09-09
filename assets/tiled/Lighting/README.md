# Tile lighting

Each original sprite in `../Sprites` has matching `.albedo.png`, `.normal.png`, and `.height.png` here. Originals and their alpha silhouettes are unchanged. Tiled continues to show the original artwork; the game uses these maps when a Sun is present.

In `Demo.tmx`, move the **Sun** point on the **Lighting** object layer. Its float properties are:

- `height`: elevation toward the viewer above the projected map plane (default 800; must be positive).
- `ambient`: minimum light on every face (0–1, default 0.10).
- `intensity`: strength of the point light (0–4, default 0.12).

Reload the client after editing. With no Sun object, the game uses the original sprites. The point affects tile lighting, not character sprites or spell effects. The sun supplies surface shading only; player and spell lights cast height-aware shadows.

Stair normals use explicit tread, riser, and side geometry so baked seam shadows cannot reverse their facing direction. Other maps are approximations reconstructed from the source art's orange face palette, with nearby structural faces used for inset materials. Albedo conversion uses a single exposure multiplier per pixel to preserve original RGB ratios and fine detail; it does not replace colors with a fixed palette. They are not recovered 3D geometry: curved surfaces and baked shadows in recesses remain approximate. Normals use view-space X right, Y down-screen, Z toward the viewer, encoded from -1..1 into RGB 0..255. Tile flips also transform their normals.

To regenerate these editable starting maps from the original sprites, run from the repository root:

```sh
cmake --build build --target generate_tile_lighting
./build/generate_tile_lighting assets/tiled/Sprites
```

Regeneration overwrites the derived PNGs, so preserve any manual improvements first.

The demo sun is deliberately dim. Living players and bots emit a warm light with a 400 pixel radius and 0.6 intensity. Spell targets emit blue lightning or orange fire light during windup; impacts brighten it and fade with the effect. Lightning spots remain lit throughout the strike. Spell light radii scale with their configured attack radii. These lights shade the tile normal maps with a smooth radial falloff and an even mix of ambient and directional illumination; a world height field blocks them only where geometry intersects the light ray. Overlapping lights are capped at the albedo brightness to avoid washed-out highlights.

Height maps encode surface elevation in red (channel value × 2 projected pixels), the lower bound of solid geometry in green, and the original silhouette in alpha. Stair heights follow the tread/riser geometry. Other shapes use approximate authored heights: walls 160, crates 80, fence posts 40 (rails at 22 and 32). Doorway lintels carry a raised lower bound so light can pass underneath.

At level load, surface pixels are projected back to their ground positions (screen Y + elevation) and accumulated into a four-pixel world grid of solid height intervals. Local lights sit 80 elevation units above their ground positions. The shader reconstructs receiver height, traces up to 127 samples toward each light, and blocks only samples inside a solid interval. Collision polygons are no longer used for lighting.

This remains approximate 2.5D geometry: one solid interval per ground cell cannot describe multiple stacked openings, and curved/recessed surfaces are simplified. Shadow rays use a three-unit surface bias to avoid self-shadow artifacts. Sunlight is still unshadowed. Height-field geometry is built when the level loads; restart after editing tiles.

Stairs use continuous stepped volumes for shadow intersections and continuous receiver elevations, rather than the quantized world grid. This prevents striped self-shadows on risers. All three stair shapes and their Tiled flips share the same geometric coordinate transform; their height textures remain available as editable reference maps, but the stair shader uses the explicit geometry. Other tiles continue to use the height field. Fence rails have explicit elevated lower bounds and follow the fence baseline, keeping their heights consistent along the sprite.

Configure local lights in `client.toml`: `[light_player]`, `[light_bot]`, `[light_explosion_windup]`, `[light_explosion_impact]`, `[light_lightning_spot]`, and `[light_lightning_impact]`. Each supports `enabled`, `radius`, `intensity`, `height`, `red`, `green`, `blue`, `radius_multiplier`, `directionality`, and `falloff_exponent`. Directionality 0 gives an even glow; 1 uses only directional normal shading. Lower falloff exponents spread the glow, higher values concentrate it. Spell radii use max(radius, spell damage radius × radius_multiplier); set the multiplier to 0 for a fixed radius. Impact lights retain their effect-driven fade. These visual settings are client-local and apply on restart; they do not change gameplay.

Local light falloff uses exp(-3 × falloff_exponent × (distance/radius)²), multiplied by a smooth taper across the outer 40% of the radius. The exponent adjusts the Gaussian concentration without sharpening the final cutoff. Player and bot defaults are radius 600, intensity 0.4, exponent 2; client TOML values override those defaults.
