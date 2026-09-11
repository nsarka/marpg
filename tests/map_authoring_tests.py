"""Run map generators in a temporary project; never overwrite hand-edited levels."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import json
import xml.etree.ElementTree as E

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='marpg-authoring-') as directory:
    project = Path(directory)
    (project / 'tools').mkdir()
    (project / 'assets/tiled').mkdir(parents=True)
    pack = 'Fantasy tileset - 2D Isometric'
    (project / 'assets' / pack).symlink_to(root / 'assets' / pack, target_is_directory=True)
    shutil.copy(root / 'assets/tiled/fantasy_pivots.json', project / 'assets/tiled')
    for name in ['map_authoring.py', 'refresh_prefab_previews.py', 'fantasy_collisions.py', 'fantasy_categories.py', 'fantasy_tileset.py', 'generate_farms.py', 'generate_fantasy_demo.py', 'generate_city.py']:
        shutil.copy(root / 'tools' / name, project / 'tools')
    for name in ['generate_farms.py', 'generate_fantasy_demo.py', 'generate_city.py']:
        subprocess.run([sys.executable, str(project / 'tools' / name)], check=True)
    for name in ['farms.tmx','demo.tmx','city.tmx']:
        refs=E.parse(project/'assets/tiled'/name).findall('tileset')
        environment=[r.get('source') for r in refs if not r.get('source').startswith('gallery/')]
        assert len(environment)==10 and all(s.startswith('fantasy_') for s in environment), (name,environment)
    city=E.parse(project/'assets/tiled/city.tmx').getroot()
    instances=city.findall("objectgroup[@name='Prefabs']/object")
    previews=city.findall('group')
    assert len(instances)==8 and len(previews)==8
    for instance in instances:
        path=project/'assets/tiled'/instance.find("properties/property[@name='prefab']").get('value')
        prefab=E.parse(path).getroot()
        roofs=prefab.findall("objectgroup[@name='Roof']")
        assert len(roofs)==1 and len(roofs[0].findall('object'))==28
        assert roofs[0].find("properties/property[@name='collision']").get('value')=='false'
    before=(project/'assets/tiled/city.tmx').read_bytes()
    subprocess.run([sys.executable,str(project/'tools/refresh_prefab_previews.py'),str(project/'assets/tiled/city.tmx')],check=True)
    assert (project/'assets/tiled/city.tmx').read_bytes()==before, 'Preview refresh must be deterministic'
    category_paths=sorted((project/'assets/tiled').glob('*.tsx'))
    assert len(category_paths)==10
    tiles={}
    for path in category_paths:
        category=E.parse(path)
        ids={int(t.get('id')) for t in category.findall('tile')}
        assert ids==set(range(len(ids))),path
        for tile in category.findall('tile'):
            source=tile.find('image').get('source').split('2D Isometric/',1)[1]
            assert source not in tiles,source
            tiles[source]=tile
            for frame in tile.findall('animation/frame'):
                assert int(frame.get('tileid')) in ids,(path,frame.attrib)
    assert len(tiles)==2780
    for source,tile in tiles.items():
        assert tile.find("properties/property[@name='collision_profile']") is not None,source
        for polygon in tile.findall('objectgroup/object/polygon'):
            points=[tuple(map(float,p.split(','))) for p in polygon.get('points').split()]
            turns=[]
            for i in range(len(points)):
                a,b,c=points[i-2],points[i-1],points[i]
                turns.append((b[0]-a[0])*(c[1]-b[1])-(b[1]-a[1])*(c[0]-b[0]))
            assert all(t>0 for t in turns) or all(t<0 for t in turns),source
    for source in ['Environment/Tree A1_E.png','Environment/Wall A1_N.png','Environment/Chest A1_E.png',
                   'Environment/Misc B42_E.png','Environment/Roof A3_E.png',
                   'Animations/Animated Tiles/WindMill1/0001.png']:
        assert tiles[source].find('objectgroup') is not None,source
    for source in ['Environment/Ground A1_E.png','Environment/Misc E11_E.png',
                   'Environment/Stone A6_E.png','Environment/Door A2_E.png']:
        assert tiles[source].find('objectgroup') is None,source
    for source,tile in tiles.items():
        if '/Portal' in source or 'Water Ripples' in source:
            assert tile.find('objectgroup') is None,source
    manifest = json.loads((project / 'assets/tiled/gallery/manifest.json').read_text())
    assert len(manifest['exhibits']) == 2108
    for path in (project / 'assets/tiled').rglob('*.tsx'):
        for image in E.parse(path).findall('.//image'):
            assert (path.parent / image.get('source')).is_file(), image.get('source')
    print('PASS: category tilesets, convex solid footprints, walkable floors/portals, complete gallery inventory and image references')

# Exercise category generation with sparse local IDs and custom metadata.
sys.path.insert(0,str(root/'tools'))
from fantasy_categories import split_collection
with tempfile.TemporaryDirectory(prefix='marpg-category-generation-') as directory:
    collection=E.fromstring("""<tileset tilewidth="256" tileheight="256" columns="0">
      <tileoffset x="-64" y="14"/>
      <tile id="17"><image source="../Fantasy tileset - 2D Isometric/Animations/Animated Tiles/WindMill1/0001.png"/>
        <properties><property name="custom" value="preserve me"/></properties>
        <objectgroup><object id="1" x="12" y="34" width="20" height="10"/></objectgroup>
        <animation><frame tileid="17" duration="80"/><frame tileid="9" duration="90"/></animation>
      </tile>
      <tile id="2"><image source="../Fantasy tileset - 2D Isometric/Environment/Ground A1_E.png"/></tile>
      <tile id="9"><image source="../Fantasy tileset - 2D Isometric/Animations/Animated Tiles/WindMill1/0002.png"/></tile>
    </tileset>""")
    catalog=split_collection(collection,Path(directory))
    animated=E.parse(Path(directory)/'fantasy_animated_objects.tsx')
    assert [f.get('tileid') for f in animated.findall('tile/animation/frame')]==['0','1']
    assert [f.get('duration') for f in animated.findall('tile/animation/frame')]==['80','90']
    assert animated.find('tile/properties/property').get('value')=='preserve me'
    assert animated.find('tile/objectgroup/object').get('x')=='12'
    print('PASS: category generation preserves collision metadata and animation timing')
