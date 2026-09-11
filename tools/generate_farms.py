#!/usr/bin/env python3
"""Rebuild the two-farm, three-route map. Regeneration replaces manual farms edits."""
from pathlib import Path
import random
from map_authoring import write, csv_layer
from refresh_prefab_previews import refresh
from fantasy_categories import CATEGORIES
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/tiled'
PACK = ROOT / 'assets/Fantasy tileset - 2D Isometric'
W, H = 64, 40
rng = random.Random(2718)

# Read authored tilesets without overwriting collision edits.
if not all((OUT / f'fantasy_{category}.tsx').exists() for category in CATEGORIES):
    from fantasy_tileset import build
    build(ROOT)
lookup, sets, first = {}, [], 1
for category in CATEGORIES:
    filename = f'fantasy_{category}.tsx'
    tileset = E.parse(OUT / filename).getroot()
    sets.append((first, filename))
    for tile in tileset.findall('tile'):
        source = tile.find('image').get('source').split('2D Isometric/', 1)[1]
        lookup[source] = first + int(tile.get('id'))
    first += max(int(tile.get('id')) for tile in tileset.findall('tile')) + 1
ids={Path(source).stem:gid for source,gid in lookup.items() if source.startswith('Environment/')}
ids['Windmill']=lookup['Animations/Animated Tiles/WindMill1/0001.png']
floor_start=ids['Ground A2_E']
roof_start=ids['Roof A3_E'];roof_id=0
layers = {name: [0] * (W * H) for name in ['Floor', 'GroundDetails', 'Paving', 'Walls', 'Roofs', 'Portals']}
layers['Floor'] = [floor_start] * (W * H)
def put(layer, x, y, tile): layers[layer][y * W + x] = tile

def road(x, y):
    return any(abs(y - route) <= 1 and 8 <= x <= 55 for route in (7, 20, 33)) or (8 <= x <= 10 or 51 <= x <= 53) and 7 <= y <= 33

for y in range(H):
    for x in range(W):
        if road(x, y): put('Floor', x, y, ids['Ground A1_E'])
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
                put('Floor', x, y, ids['Ground A1_E'])
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

# Forest hamlets: clear only the rooms, courtyards and narrow access paths.
houses = [(24, 11, 'timber_brown'), (36, 11, 'stone_red'),
          (24, 25, 'log_yellow'), (36, 25, 'stone_blue')]
def clearing(x, y, paved=False):
    put('Walls', x, y, 0)
    put('GroundDetails', x, y, 0)
    put('Floor', x, y, ids['Ground A1_E'])
    if paved:
        put('Paving', x, y, ids['Ground H1_E'])

for x0, y0, prefab in houses:
    for y in range(y0-1, y0+5):
        for x in range(x0-1, x0+5):
            clearing(x, y)
for cy, route_start, route_end in [(13, 7, 20), (27, 20, 33)]:
    for y in range(route_start, route_end+1):
        clearing(32, y)
    for x in range(23, 41):
        clearing(x, cy)
    for y in range(cy-2, cy+3):
        for x in range(30, 35):
            clearing(x, y, paved=True)
    put('GroundDetails', 32, cy, ids['Stone A6_E'])
    put('Portals', 32, cy, lookup['Animations/Props/PortalIdle/0033.png'])
    # Decorations stay on the courtyard edges, leaving every approach open.
    for x, y, name in [(30,cy-2,'Misc B26_N'), (34,cy+2,'Misc A8_E')]:
        put('Walls', x, y, ids[name])

m = E.Element('map', version='1.10', tiledversion='1.12.2', orientation='isometric',
              renderorder='right-down', width=str(W), height=str(H), tilewidth='128',
              tileheight='64', infinite='0')
for first, filename in sets: E.SubElement(m,'tileset',firstgid=str(first),source=filename)
windmills=[gid if gid==ids['Windmill'] else 0 for gid in layers['Walls']]
layers['Walls']=[0 if gid==ids['Windmill'] else gid for gid in layers['Walls']]
layer_id = 1
for name,data in layers.items():
    layer=csv_layer(m,layer_id,name,W,H,data)
    layer_id += 1
    if name=='Walls':
        extra=csv_layer(m,layer_id,'Walls - windmills',W,H,windmills)
        layer_id += 1
        extra.set('offsetx','-128');extra.set('offsety','46')
    if name=='Roofs':layer.set('offsety','-96')
spawns = E.SubElement(m,'objectgroup',id=str(layer_id),name='Spawns')
layer_id += 1
# Sixteen points per farm allow every supported team count; two teams cluster by farm.
for i in range(32):
    x = (5 if i < 16 else 48) + i % 4
    y = 19 + (i % 16) // 4
    assert layers['Walls'][y*W+x] == 0
    obj = E.SubElement(spawns,'object',id=str(i+1),name=f'Farm {1+i//16} spawn {1+i%16}',x=str(x*64+64),y=str(y*64))
    E.SubElement(obj,'point')
triggers = E.SubElement(m,'objectgroup',id=str(layer_id),name='Triggers')
layer_id += 1
for object_id, cy, name, destination in [(33,13,'north_courtyard','south_courtyard'),
                                        (34,27,'south_courtyard','north_courtyard')]:
    obj=E.SubElement(triggers,'object',id=str(object_id),name=name,type='Teleport',
                     x=str(32*64+12),y=str(cy*64+12),width='40',height='40')
    props=E.SubElement(obj,'properties')
    E.SubElement(props,'property',name='destination',value=destination)
instances = E.SubElement(m,'objectgroup',id=str(layer_id),name='Prefabs')
layer_id += 1
for object_id, (x,y,prefab) in enumerate(houses,35):
    obj=E.SubElement(instances,'object',id=str(object_id),name=prefab,x=str(x*64),y=str(y*64))
    E.SubElement(obj,'point')
    props=E.SubElement(obj,'properties')
    E.SubElement(props,'property',name='prefab',type='file',value=f'prefabs/{prefab}.tmx')
m.set('nextlayerid',str(layer_id));m.set('nextobjectid','39')
write(m, OUT / 'farms.tmx')
refresh(OUT / 'farms.tmx')
print('Created farms: 64x40, two farms, three routes, four forest houses, linked portal courtyards, 32 safe spawn points.')
