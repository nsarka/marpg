# Fantasy category tilesets

Add the categories you need as external tilesets in Tiled:

| File | Contents |
| --- | --- |
| `fantasy_ground.tsx` | Ground, paths, terrain and cliffs |
| `fantasy_walls.tsx` | Walls, fences and structural beams |
| `fantasy_roofs.tsx` | Roof pieces |
| `fantasy_doors.tsx` | Doors and gates |
| `fantasy_vegetation.tsx` | Trees, plants and wall vegetation |
| `fantasy_stone.tsx` | Rocks, ore and stone structures |
| `fantasy_props.tsx` | Furniture, containers, farm props and static torches |
| `fantasy_animated_objects.tsx` | Windmills, barrels, turrets and animated torches |
| `fantasy_portals.tsx` | Portal animations and portal platforms |
| `fantasy_effects.tsx` | Fire, water ripples, destruction effects and shadows |

Arena, farms and demo reference the category tilesets;
demo also uses the character animation tilesets in `gallery/`. Tilesets share
the source images. Each animated sequence and its
frames live in the same category tileset.

Solid tiles have Tiled collision polygons. Floors, crops, small ground
decorations, portal platforms and visual effects are passable. Edit shapes in
Tiled's Tile Collision Editor to change every placement of that tile. Press F1 in
the game to inspect collisions. The `collision_profile` property is descriptive;
the game uses the actual polygons for collision.

Category tilesets use a drawing offset of (-64, 14). Farms uses a
windmill layer offset of (-128, 46) and a raised roof layer offset of (0, -96).

`tools/fantasy_categories.py` defines the categories, and
`tools/fantasy_collisions.py` defines reproducible collision profiles.
`python3 tools/fantasy_tileset.py` regenerates the category TSX files. Map generators
also regenerate them, so incorporate manual tileset edits into the authoring code
before regeneration. Normal editing in Tiled does not require these scripts.

## City

`city.tmx` is a 64×64 settlement: Stonekeep Castle at the top, six walk-through houses around
Market Square in the middle, and six farm plots with two walk-through barns, orchards and two animated
windmills at the bottom. It includes 32 safe spawn points and a central avenue.
Set the existing `[server]` map entry to `"city"` to play it.

`python3 tools/generate_city.py` regenerates only this map from the existing
category tilesets; it does not rebuild or change their collision definitions.
Regeneration replaces manual edits to `city.tmx`.

City houses and barns are instances of editable maps in `prefabs/`. Each prefab
contains its floor, walls, furniture, and **one Roof object layer**. The roof's
native-size tile objects have individual positions and explicit drawing order.
The server and client expand the same prefab files on map load, including tile
collisions and translated roof regions. Editing a prefab updates every instance
after restarting/reloading the map. Keep the same files on server and clients.

To place an existing house:

1. Create an object layer named `Prefabs` in your map.
2. Insert a **point** and set its X and Y to multiples of 64 (for the 128×64 grid).
   This point is the prefab's cell (0,0), not the center of its floor footprint.
3. Add a custom **File** property named `prefab`, and select a map such as
   `prefabs/stone_red.tmx`. Keep its footprint inside the host map.
4. For an editor preview, import the prefab's category tilesets into your map,
   save it, and run `python3 tools/refresh_prefab_previews.py assets/tiled/YOUR_MAP.tmx`.
   Reload the map in Tiled after the command completes.

The preview command adds one locked, collapsible group per house. Its
`editor_only=true` property tells the game to ignore the cached geometry; runtime
instances always come from their source files. Rerun the command after moving a
marker or editing a prefab. Do not edit the generated previews. Save or close the
map in Tiled before running the command to avoid overwriting unsaved edits.
The city generator refreshes its previews automatically.

To make another house, duplicate a map in `prefabs/`, edit its tile layers, and
move/add tile objects in its Roof layer. Use Tiled's **Insert Tile** object tool;
keep their original image dimensions, bottom-center alignment, and zero rotation.
Use object drawing order `index` to explicitly control overlapping roof pieces.
The Roof layer has `collision=false` and `roof_region="0 0 4 4"`; update the region
for a different footprint. This rectangle is in map cells, and hides all the
building's roof objects and occlusion masks together while the player is inside.
Floors and walls continue to use their tileset collision definitions.

Prefab maps currently require finite isometric maps, CSV tile data, and external
tilesets with the same grid as their host. Instances translate without rotation
or scaling. Tile objects support native dimensions and tile flips; unsupported
rotation/scaling produces an explicit load error. Nested group layers are flattened
for the game, and prefab references can be nested (cycles are rejected).
`Floor`, `GroundDetails`, `Paving`, `Walls`, and `SmallProps` tiles merge into the
host layers by name; nonempty prefab tiles replace host tiles at those cells.
Other layers retain their order and offsets. Keep shared layers at matching offsets.
