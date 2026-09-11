"""Split a fantasy collection into category tilesets without changing tile metadata."""
from copy import deepcopy
from dataclasses import dataclass
from pathlib import PurePosixPath
import xml.etree.ElementTree as E
from map_authoring import write

CATEGORIES = {
    'ground': 'Ground',
    'walls': 'Walls and fences',
    'roofs': 'Roofs',
    'doors': 'Doors',
    'vegetation': 'Trees and vegetation',
    'stone': 'Rocks and stone structures',
    'props': 'Furniture, containers and farm props',
    'animated_objects': 'Animated objects',
    'portals': 'Portals',
    'effects': 'Visual effects',
}


def category_for(source):
    path = PurePosixPath(source)
    if source.startswith('Environment/'):
        category = path.stem.split()[0]
        if category in ('Ground', 'BLACK', 'TilePreview'):
            return 'ground'
        if category in ('Wall', 'Roof', 'Door'):
            return {'Wall': 'walls', 'Roof': 'roofs', 'Door': 'doors'}[category]
        if category in ('Tree', 'Flora', 'WallFlora', 'TreeTrunkBroken'):
            return 'vegetation'
        if category == 'Stone':
            return 'portals' if path.stem.split('_')[0] in ('Stone A6', 'Stone A7', 'Stone A8') else 'stone'
        if category.startswith('Shadow'):
            return 'effects'
        return 'props'
    if source.startswith('Animations/'):
        sequence = path.parent.name
        if sequence.startswith('Portal'):
            return 'portals'
        if sequence.startswith(('WindMill', 'Barrel ')) or sequence in ('Turret1', 'Torch 1'):
            return 'animated_objects'
    return 'effects'


@dataclass
class Catalog:
    sets: list
    lookup: dict
    old_to_gid: dict
    groups: dict
    tile_count: int
    environment_count: int


def split_collection(collection, out, groups=None, environment_count=0):
    """Keep images, collision objects, offsets and properties; remap animation IDs."""
    buckets = {key: [] for key in CATEGORIES}
    for tile in collection.findall('tile'):
        source = tile.find('image').get('source').split('2D Isometric/', 1)[1]
        buckets[category_for(source)].append(tile)
    sets, lookup, old_to_gid = [], {}, {}
    first = 1
    for key, originals in buckets.items():
        if not originals:
            continue
        tileset = E.Element('tileset', collection.attrib)
        tileset.set('name', 'Fantasy - ' + CATEGORIES[key])
        tileset.set('tilecount', str(len(originals)))
        for node in collection:
            if node.tag != 'tile':
                tileset.append(deepcopy(node))
        local_ids = {int(tile.get('id')): i for i, tile in enumerate(originals)}
        for original in originals:
            tile = deepcopy(original)
            old = int(tile.get('id'))
            tile.set('id', str(local_ids[old]))
            for frame in tile.findall('animation/frame'):
                frame.set('tileid', str(local_ids[int(frame.get('tileid'))]))
            tileset.append(tile)
            source = tile.find('image').get('source').split('2D Isometric/', 1)[1]
            lookup[source] = first + local_ids[old]
            old_to_gid[old] = first + local_ids[old]
        filename = 'fantasy_' + key + '.tsx'
        write(tileset, out / filename)
        sets.append((first, filename))
        first += len(originals)
    mapped_groups = {}
    for category, items in (groups or {}).items():
        mapped_groups[category] = [(name, old_to_gid[gid - 2], source) for name, gid, source in items]
    return Catalog(sets, lookup, old_to_gid, mapped_groups, first - 1, environment_count)
