#!/usr/bin/env python3
"""Generate city.tmx from existing category tilesets; never rebuild shared assets."""
from pathlib import Path
import random
import xml.etree.ElementTree as E
from fantasy_categories import CATEGORIES
from map_authoring import csv_layer, write
from refresh_prefab_previews import refresh

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/tiled'
W = H = 64
rng = random.Random(73421)
lookup, sets, first = {}, [], 1
for category in CATEGORIES:
    filename = f'fantasy_{category}.tsx'
    ts = E.parse(OUT / filename).getroot()
    sets.append((first, filename))
    for tile in ts.findall('tile'):
        source = tile.find('image').get('source').split('2D Isometric/', 1)[1]
        lookup[source] = first + int(tile.get('id'))
    first += max(int(t.get('id')) for t in ts.findall('tile')) + 1

layers = {name: [0] * (W * H) for name in
          ('Floor', 'GroundDetails', 'Paving', 'Walls', 'SmallProps', 'Windmills', 'Roofs', 'TowerRoofs')}
blocked = set()
landmarks = []


def gid(name):
    return lookup[name if '/' in name else 'Environment/' + name + '.png']


def put(layer, x, y, name):
    assert 0 <= x < W and 0 <= y < H, (x, y)
    layers[layer][y * W + x] = gid(name)
    if layer in ('Walls', 'SmallProps', 'Windmills', 'Roofs', 'TowerRoofs'):
        blocked.add((x, y))


def ground(x, y, name):
    put('Floor', x, y, name)
    layers['GroundDetails'][y * W + x] = 0


def rect(x0, y0, x1, y1, name):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            ground(x, y, name)


def street(x, y):
    return abs(x - y) <= 2 or (27 <= x <= 29 and 24 <= y <= 44) or (27 <= y <= 29 and 24 <= x <= 44) or (39 <= x <= 41 and 24 <= y <= 51) or (39 <= y <= 41 and 24 <= x <= 51)


# A forest-framed settlement with one clear central spine, not a rectangular carpet.
for y in range(H):
    for x in range(W):
        if 4 <= x <= 59 and 4 <= y <= 59:
            ground(x, y, 'Ground A2_E')
            if rng.random() < .12:
                put('GroundDetails', x, y, rng.choice(['Ground A7_E', 'Ground A8_N', 'Ground A10_E', 'Ground A11_S', 'Flora A1_E', 'Flora A3_N']))
            border = x < 7 or y < 7 or x > 56 or y > 56
            outskirts = abs(x - y) > 24
            if (border or outskirts and rng.random() < .32) and not street(x, y):
                put('Walls', x, y, rng.choice(['Tree A1_E', 'Tree A2_N', 'Tree A3_E', 'Tree A1_S', 'Tree A2_W']))
        if 7 <= x <= 56 and 7 <= y <= 56 and street(x, y):
            ground(x, y, 'Ground A1_E')
            if x + y < 87:
                put('Paving', x, y, 'Ground H1_E')

# Castle: outer curtain wall, four towers, open gates, a keep and a paved courtyard.
rect(10, 10, 24, 24, 'Ground A1_E')
for y in range(10, 25):
    for x in range(10, 25):
        put('Paving', x, y, 'Ground H1_E')
        layers['Walls'][y * W + x] = 0
        blocked.discard((x, y))
for n in range(11, 24):
    put('Walls', n, 10, 'Wall A1_N')
    put('Walls', 10, n, 'Wall A1_E')
    if n not in (20, 21, 22, 23):
        put('Walls', n, 24, 'Wall A7_N' if n % 3 == 0 else 'Wall A1_N')
        put('Walls', 24, n, 'Wall A7_E' if n % 3 == 0 else 'Wall A1_E')
for x, y, facing in [(10, 10, 'W'), (24, 10, 'N'), (10, 24, 'S'), (24, 24, 'E')]:
    # Leave the main diagonal gateway open at the front corner.
    if (x, y) == (24, 24):
        continue
    put('Walls', x, y, 'Wall A2_' + facing)
    put('TowerRoofs', x, y, 'Roof D7_' + facing)
for x, y in [(13, 13), (14, 13), (15, 13), (13, 14), (14, 14), (15, 14), (13, 15), (14, 15), (15, 15)]:
    put('Walls', x, y, 'Wall A2_E')
    put('Roofs', x, y, 'Roof D5_E')
for x, y, name in [(12, 19, 'Misc C5_E'), (19, 12, 'Misc C6_N'), (17, 12, 'Misc D1_E'),
                   (12, 17, 'Misc D2_N'), (19, 16, 'Chest A2_E'), (16, 19, 'Chest A3_N'),
                   (20, 14, 'Misc C4_E'), (14, 20, 'Misc B21_N'), (22, 18, 'Stone A4_E'),
                   (18, 22, 'Stone A5_N'), (19, 24, 'Torch East'), (24, 19, 'Torch West')]:
    put('SmallProps', x, y, name)
landmarks.append(('Stonekeep Castle', 17, 17))

# Market plaza with stalls along its perimeter, leaving the center open for combat.
rect(30, 30, 38, 38, 'Ground A1_E')
for y in range(30, 39):
    for x in range(30, 39):
        put('Paving', x, y, 'Ground H1_E')
for x, y, name in [(30, 33, 'Misc B10_E'), (30, 36, 'Misc B14_E'), (33, 30, 'Misc B17_N'),
                   (36, 30, 'Misc B18_N'), (38, 33, 'Misc B19_W'), (33, 38, 'Misc B12_S'),
                   (38, 36, 'Misc B20_E'), (36, 38, 'Misc B7_N'), (30, 30, 'Misc B42_E')]:
    put('SmallProps', x, y, name)
landmarks.append(('Market Square', 34, 34))

# Large houses are assembled after district decoration so their rooms stay clear.
houses = [(21,30), (30,21), (21,39), (39,21), (32,43), (43,32)]
roof_layers = {}

# Southern farms: six plots, crop variation within each plot, open fence gates.
for index, (x0, y0) in enumerate([(42, 47), (42, 52), (47, 52), (47, 42), (52, 42), (52, 47)]):
    crop = ['Misc E11_E', 'Misc E8_N', 'Misc E5_E', 'Ground J1_E', 'Ground A3_E', 'Ground A5_E'][index]
    for y in range(y0, y0 + 3):
        for x in range(x0, x0 + 3):
            ground(x, y, 'Ground A1_E')
            put('GroundDetails', x, y, crop)
    for n in range(3):
        put('Walls', x0 + n, y0 + 3, 'Wall B7_N')
        put('Walls', x0 + 3, y0 + n, 'Wall B7_E')
    # Dirt access lanes behind each field are intentionally free of props.
for x, y, name in [(45, 49, 'Misc E1_E'), (49, 45, 'Misc E2_N'), (45, 54, 'Misc E3_E'),
                   (54, 45, 'Misc E4_N'), (50, 55, 'Misc B5_E'), (55, 50, 'Misc B6_N'),
                   (44, 46, 'Misc A3_E'), (46, 44, 'Misc A9_N'), (54, 54, 'Misc B42_E')]:
    put('SmallProps', x, y, name)
for x, y, mill in [(38, 49, 'WindMill1'), (49, 38, 'WindMill2')]:
    put('Windmills', x, y, f'Animations/Animated Tiles/{mill}/0001.png')
landmarks.append(('Southfields', 50, 50))

# Small workshops, orchard rows and boulders frame the inhabited streets.
for x, y, name in [(19, 29, 'Misc C1_E'), (29, 19, 'Misc C2_N'), (19, 33, 'Misc C3_E'),
                   (33, 19, 'Misc B11_N'), (19, 37, 'Misc B8_E'), (37, 19, 'Misc B9_N'),
                   (26, 44, 'Misc B41_E'), (44, 26, 'Misc B43_N'), (27, 47, 'Misc B58_E'),
                   (47, 27, 'Misc C9_N')]:
    put('SmallProps', x, y, name)
for x, y in [(35, 50), (35, 53), (35, 56), (50, 35), (53, 35), (56, 35)]:
    put('Walls', x, y, rng.choice(['Tree A1_E', 'Tree A2_N', 'Tree A3_W']))
for x, y, name in [(8, 28, 'Stone A1_E'), (28, 8, 'Stone A2_N'), (12, 32, 'Stone A3_S'),
                   (32, 12, 'Stone A9_W'), (16, 40, 'Stone A10_E'), (40, 16, 'Stone A12_N')]:
    put('SmallProps', x, y, name)

# Main street is guaranteed clear from castle gate to the southern entrance.
for y in range(7, 57):
    for x in range(7, 57):
        if abs(x - y) <= 2 and x + y >= 34:
            for layer in ('Walls', 'SmallProps', 'Windmills', 'Roofs', 'TowerRoofs'):
                layers[layer][y * W + x] = 0
            blocked.discard((x, y))

# Eight accessible buildings: six town houses and two farm barns. A four-by-four
# footprint leaves a two-by-two room, with opposing entrances and a clear aisle.
buildings = houses + [(38,52), (52,38)]
for index, (x0,y0) in enumerate(buildings):
    family=['D','F','G','D','F','C','C','C'][index]
    roof=['C','D','E','C','D','F','A','A'][index]
    for y in range(y0,y0+4):
        for x in range(x0,x0+4):
            for layer in layers:
                layers[layer][y*W+x]=0
            blocked.discard((x,y))
            ground(x,y,'Ground A1_E')
            put('Paving',x,y,'Ground F1_E')
            # Wall segments form the perimeter. Both x faces have a one-cell entrance.
            if y in (y0,y0+3):
                put('Walls',x,y,f'Wall {family}1_'+('N' if y==y0 else 'S'))
            elif x in (x0,x0+3) and y!=y0+2:
                put('Walls',x,y,f'Wall {family}1_'+('W' if x==x0 else 'E'))
    for x in (x0-1,x0+4):
        ground(x,y0+2,'Ground A1_E')
        for layer in ('Walls','SmallProps'):
            layers[layer][(y0+2)*W+x]=0
    put('SmallProps',x0+1,y0+1,['Chest A1_E','Misc B26_N','Misc A8_E','Misc B28_E',
                              'Chest A2_N','Misc B20_E','Misc E1_E','Misc E2_N'][index])
    name=f'House {index+1} roof' if index<6 else f'Barn {index-5} roof'
    # Close both gable ends with the pack's timber panels. These offsets align
    # the inset panel edges with the slope overhang without resizing artwork.
    base = f'{name} gable base'
    layers[base] = [0] * (W * H)
    for end in (y0 - 1, y0 + 3):
        for column in (1, 2):
            put(base, x0 + column, end, 'Roof B1_E')
    roof_layers[base] = (x0, y0, -6, -110)
    for column in range(4):
        panel = f'{name} gable {column + 1}'
        layers[panel] = [0] * (W * H)
        for end in (y0 - 1, y0 + 3):
            put(panel, x0 + column, end, 'Roof B2_' + ('W' if column < 2 else 'E'))
        elevation = min(column, 3 - column) * 64
        roof_layers[panel] = (x0, y0, -56 if column < 2 else -6,
                              -110 - elevation + (32 if column < 2 else 0))
    # Each column is a strip of four native-size slope tiles. The inner strips
    # rise 64 pixels to meet at the ridge; opposite slopes use opposite facings.
    # Separate layers give Tiled the same elevation offsets as the game.
    for column in range(4):
        strip = f'{name} slope {column + 1}'
        layers[strip] = [0] * (W * H)
        for row in range(4):
            put(strip, x0 + column, y0 + row,
                f'Roof {roof}1_' + ('W' if column < 2 else 'E'))
        roof_layers[strip] = (x0, y0, 0, -96 - min(column, 3 - column) * 64)

# Save each house as a reusable, independently editable map. Roof tile objects
# carry their own positions instead of needing a layer for every elevation.
prefabs = []
(OUT / 'prefabs').mkdir(exist_ok=True)
for index, (x0, y0) in enumerate(buildings):
    filename = ['stone_red', 'stone_blue', 'timber_brown', 'stone_red_chairs',
                'stone_blue_chest', 'log_yellow', 'barn_hay', 'barn_straw'][index] + '.tmx'
    prefab = E.Element('map', version='1.10', tiledversion='1.12.2', orientation='isometric',
                       renderorder='right-down', width='4', height='4', tilewidth='128', tileheight='64', infinite='0')
    for firstgid, source in sets:
        E.SubElement(prefab, 'tileset', firstgid=str(firstgid), source='../' + source)
    for layer_id, name in enumerate(('Floor', 'Paving', 'Walls', 'SmallProps'), 1):
        data = [layers[name][(y0+y)*W+x0+x] for y in range(4) for x in range(4)]
        csv_layer(prefab, layer_id, name, 4, 4, data)
        if name != 'Floor':
            for y in range(4):
                for x in range(4):
                    layers[name][(y0+y)*W+x0+x] = 0
    roof = E.SubElement(prefab, 'objectgroup', id='5', name='Roof', draworder='index')
    props = E.SubElement(roof, 'properties')
    E.SubElement(props, 'property', name='collision', type='bool', value='false')
    E.SubElement(props, 'property', name='roof_region', value='0 0 4 4')
    object_id = 1
    for name, (rx, ry, dx, dy) in list(roof_layers.items()):
        if (rx, ry) != (x0, y0):
            continue
        for cell, tile in enumerate(layers.pop(name)):
            if not tile:
                continue
            x, y = cell % W - x0, cell // W - y0
            # Isometric tile objects use a bottom-center anchor; category tile
            # layers use bottom-left alignment. Convert their visible positions.
            px, py = (x-y)*64 + dx + 64, (x+y)*32 + dy + 64
            E.SubElement(roof, 'object', id=str(object_id), gid=str(tile),
                         x=str(py+px/2), y=str(py-px/2), width='256', height='256')
            object_id += 1
        del roof_layers[name]
    prefab.set('nextlayerid', '6'); prefab.set('nextobjectid', str(object_id))
    write(prefab, OUT / 'prefabs' / filename)
    prefabs.append((filename, x0, y0))

m = E.Element('map', version='1.10', tiledversion='1.12.2', orientation='isometric',
              renderorder='right-down', width=str(W), height=str(H), tilewidth='128', tileheight='64', infinite='0')
for firstgid, source in sets:
    E.SubElement(m, 'tileset', firstgid=str(firstgid), source=source)
for index, (name, tiles) in enumerate(layers.items(), 1):
    layer = csv_layer(m, index, name, W, H, tiles)
    if name == 'Windmills':
        layer.set('offsetx', '-128'); layer.set('offsety', '46')
    if name == 'Roofs': layer.set('offsety', '-96')
    if name == 'TowerRoofs': layer.set('offsety', '-128')
    if name in roof_layers:
        x,y,dx,dy=roof_layers[name]
        layer.set('offsetx',str(dx))
        layer.set('offsety',str(dy))
        props=E.SubElement(layer,'properties')
        E.SubElement(props,'property',name='collision',type='bool',value='false')
        E.SubElement(props,'property',name='roof_region',value=f'{x} {y} 4 4')
instances = E.SubElement(m, 'objectgroup', id=str(len(layers)+4), name='Prefabs')
for i, (filename, x, y) in enumerate(prefabs, 36):
    obj = E.SubElement(instances, 'object', id=str(i), name=filename[:-4], x=str(x*64), y=str(y*64))
    E.SubElement(obj, 'point')
    props = E.SubElement(obj, 'properties')
    E.SubElement(props, 'property', name='prefab', type='file', value='prefabs/' + filename)
spawns = E.SubElement(m, 'objectgroup', id=str(len(layers)+1), name='Spawns')
# Two clusters on the central avenue; spatial grouping supports teams and FFA.
for i in range(32):
    base = 25 if i < 16 else 42
    step, side = (i % 16) // 2, i % 2
    x, y = base + step + side, base + step
    assert (x, y) not in blocked
    obj = E.SubElement(spawns, 'object', id=str(i + 1), name=f'City spawn {i+1}', x=str(x * 64 + 64), y=str(y * 64))
    E.SubElement(obj, 'point')
E.SubElement(m, 'objectgroup', id=str(len(layers)+2), name='Triggers')
labels = E.SubElement(m, 'objectgroup', id=str(len(layers)+3), name='Labels')
for i, (name, x, y) in enumerate(landmarks, 33):
    E.SubElement(labels, 'object', id=str(i), name=name, x=str(x * 64 + 64), y=str(y * 64))
m.set('nextlayerid', str(len(layers)+5)); m.set('nextobjectid', '44')
write(m, OUT / 'city.tmx')
refresh(OUT / 'city.tmx')
print(f'Created city: {len(houses)} houses, castle, six farm plots, two windmills, 32 spawn points; '
      f'{len(set(g for layer in layers.values() for g in layer if g))} distinct tiles')
