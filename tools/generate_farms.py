#!/usr/bin/env python3
"""Rebuild the two-farm, three-route map. Regeneration replaces manual farms edits."""
from pathlib import Path
import random
from map_authoring import write, set_pivot, image_tile, csv_layer
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/tiled'
PACK = ROOT / 'assets/Fantasy tileset - 2D Isometric'
W, H = 64, 40
rng = random.Random(2718)

sets = []
next_gid = 1

def tileset(name, offset=14):
    node = E.Element('tileset', version='1.10', tiledversion='1.12.2', name=name,
                     tilewidth='256', tileheight='256', columns='0')
    set_pivot(node,(.5,.18))
    node.find('tileoffset').set('y',str(offset))
    return node

def add(ts, filename, polygon=None):
    idx = len(ts.findall('tile'))
    assert (PACK / filename).is_file(), filename
    node = image_tile(ts,idx,'../Fantasy tileset - 2D Isometric/'+filename,PACK / filename)
    if polygon:
        group = E.SubElement(node, 'objectgroup', draworder='index')
        obj = E.SubElement(group, 'object', id='1', name='Solid footprint', x='0', y='0')
        E.SubElement(obj, 'polygon', points=polygon)
    return idx

def finish(ts, filename):
    global next_gid
    count = len(ts.findall('tile'))
    ts.set('tilecount', str(count))
    write(ts, OUT / filename)
    first = next_gid
    sets.append((first, filename))
    next_gid += count
    return first

floor_ts = tileset('Farms terrain')
for name in ['Ground A2_E', 'Ground A1_E']:
    add(floor_ts, 'Environment/' + name + '.png')
floor_start = finish(floor_ts, 'farms_ground.tsx')
props_ts = tileset('Farms scenery')
ids = {}
full = '64,210 128,178 192,210 128,242'
small = '104,210 128,198 152,210 128,222'
for name in ['Tree A1_E', 'Tree A2_E', 'Tree A3_E', 'Tree A1_N',
             'Wall B7_E', 'Wall B7_N', 'Wall C2_E', 'Wall G3_E',
             'Misc E1_E', 'Misc E2_E', 'Misc E4_E', 'Misc B5_E', 'Misc B6_E',
             'Misc B42_E', 'Misc A8_E', 'Misc B1_E', 'Misc B52_E',
             'Misc E5_E', 'Misc E6_E', 'Misc E11_E', 'Ground A22_E', 'Ground A10_E', 'Ground A7_E']:
    polygon = full if name.startswith(('Tree', 'Wall C', 'Wall G')) else None
    if name == 'Wall B7_E': polygon = '64,230 72,234 192,174 184,170'
    if name == 'Wall B7_N': polygon = '64,174 72,170 192,230 184,234'
    if name.startswith(('Misc E1_', 'Misc E2_', 'Misc E4_', 'Misc B5_', 'Misc B6_', 'Misc B42_')): polygon = small
    ids[name] = add(props_ts, 'Environment/' + name + '.png', polygon)
props_start = finish(props_ts, 'farms_scenery.tsx')
ids = {name: idx + props_start for name, idx in ids.items()}
# Windmills are 512x512, unlike the 256x256 environment tiles.
wind_ts = tileset('Farms animated windmill', 60)
wind_ts.set('tilewidth','512');wind_ts.set('tileheight','512')
wind_ts.find('tileoffset').set('x','-192')
frames = sorted((PACK / 'Animations/Animated Tiles/WindMill1').glob('*.png'))
windmill_ids = [add(wind_ts, f.relative_to(PACK).as_posix(), '216,420 256,400 296,420 256,440') for f in frames]
anim = E.SubElement(wind_ts.findall('tile')[0], 'animation')
for idx in windmill_ids: E.SubElement(anim, 'frame', tileid=str(idx), duration='80')
ids['Windmill'] = finish(wind_ts, 'farms_windmill.tsx')
roof_ts = tileset('Farms thatched roofs', -82)
# Roof artwork is raised 96 pixels; the collision polygon remains at ground level.
roof_id = add(roof_ts, 'Environment/Roof A3_E.png', '64,306 128,274 192,306 128,338')
roof_start = finish(roof_ts, 'farms_roofs.tsx')

layers = {name: [0] * (W * H) for name in ['Floor', 'GroundDetails', 'Walls', 'Roofs']}
layers['Floor'] = [floor_start] * (W * H)
def put(layer, x, y, tile): layers[layer][y * W + x] = tile

def road(x, y):
    return any(abs(y - route) <= 1 and 8 <= x <= 55 for route in (7, 20, 33)) or (8 <= x <= 10 or 51 <= x <= 53) and 7 <= y <= 33

for y in range(H):
    for x in range(W):
        if road(x, y): put('Floor', x, y, floor_start + 1)
        # Impassable forest belts separate the three routes; bases have open courtyards.
        border = x < 3 or x >= W - 3 or y < 3 or y >= H - 3
        belt = 20 <= x <= 43 and (10 + (x%5==0) <= y <= 16 - (x%7==0) or 24 + (x%7==0) <= y <= 30 - (x%5==0))
        if border or belt:
            put('Walls', x, y, ids[rng.choice(['Tree A1_E', 'Tree A2_E', 'Tree A3_E', 'Tree A1_N'])])
        elif not road(x, y) and rng.random() < .11:
            put('GroundDetails', x, y, ids[rng.choice(['Ground A22_E', 'Ground A10_E', 'Ground A7_E'])])

# Matching farm layouts with wheat fields, barns, sheds, wells, carts and hay stores.
for base in (0, 43):
    for y0 in (10, 26):
        for y in range(y0, y0 + 5):
            for x in range(base + 11, base + 17):
                put('Floor', x, y, floor_start + 1)
                put('GroundDetails', x, y, ids['Misc E11_E'])
        for x in range(base + 11, base + 17):
            put('Walls', x, y0 + 5, ids['Wall B7_N'])
        for y in range(y0, y0 + 5):
            put('Walls', base + 17, y, ids['Wall B7_E'])
    for x,y,wall in [(base+5,13,'Wall C2_E'),(base+5,27,'Wall C2_E')]:
        put('Walls', x,y,ids[wall]);put('Roofs',x,y,roof_start+roof_id)
    for x,y,name in [(base+15,17,'Windmill'),(base+5,18,'Misc B42_E'),
                     (base+12,23,'Misc B5_E'),(base+16,23,'Misc E2_E'),
                     (base+17,22,'Misc E4_E'),(base+7,15,'Misc A8_E'),
                     (base+7,28,'Misc B1_E'),(base+12,18,'Misc B52_E'),
                     (base+4,25,'Misc E1_E')]:
        put('Walls',x,y,ids[name])

m = E.Element('map', version='1.10', tiledversion='1.12.2', orientation='isometric',
              renderorder='right-down', width=str(W), height=str(H), tilewidth='128',
              tileheight='64', infinite='0', nextlayerid='8', nextobjectid='33')
for first, filename in sets: E.SubElement(m,'tileset',firstgid=str(first),source=filename)
for lid,(name,data) in enumerate(layers.items(),1):
    csv_layer(m,lid,name,W,H,data)
spawns = E.SubElement(m,'objectgroup',id='5',name='Spawns')
# Sixteen points per farm allow every supported team count; two teams cluster by farm.
for i in range(32):
    x = (5 if i < 16 else 48) + i % 4
    y = 19 + (i % 16) // 4
    assert layers['Walls'][y*W+x] == 0
    obj = E.SubElement(spawns,'object',id=str(i+1),name=f'Farm {1+i//16} spawn {1+i%16}',x=str(x*64+64),y=str(y*64))
    E.SubElement(obj,'point')
E.SubElement(m,'objectgroup',id='6',name='Triggers')
write(m, OUT / 'farms.tmx')
print('Created farms: 64x40, two farms, three routes, 32 safe spawn points.')
