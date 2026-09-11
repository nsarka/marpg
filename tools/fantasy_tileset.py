"""Build the fantasy category tilesets from the full environment asset collection."""
from pathlib import Path
import re
import xml.etree.ElementTree as E
from map_authoring import set_pivot, image_tile, write
from fantasy_collisions import apply_collisions
from fantasy_categories import split_collection


def natural(path):
    return [int(v) if v.isdigit() else v.lower() for v in re.split(r'(\d+)',str(path))]


def build(root):
    pack=root/'assets/Fantasy tileset - 2D Isometric'
    out=root/'assets/tiled';out.mkdir(parents=True,exist_ok=True)
    collection=E.Element('tileset',version='1.10',tiledversion='1.12.2',name='Fantasy pack gallery',tilewidth='256',tileheight='256',columns='0')
    set_pivot(collection,(.5,.18))
    nodes=[];groups={}
    def tile(path):
        source=path.relative_to(pack).as_posix()
        node=image_tile(collection,len(nodes),'../Fantasy tileset - 2D Isometric/'+source,path)
        apply_collisions(node,source);nodes.append(node)
        return len(nodes)-1
    environment=sorted((pack/'Environment').glob('*.png'),key=natural)
    for path in environment:
        category=path.stem.split()[0]
        if category.startswith('Shadow') or category in ('TilePreview','BLACK','Torch2','FirePlace','TreeTrunkBroken'):category='Other environment'
        groups.setdefault('Environment / '+category,[]).append((path.stem,tile(path)+2,path.relative_to(pack).as_posix()))
    for folder in sorted((pack/'Animations').glob('*/*'),key=natural):
        frames=sorted(folder.glob('*.png'),key=natural)
        if not frames:continue
        ids=[tile(p) for p in frames];animation=E.SubElement(nodes[ids[0]],'animation')
        for i in ids:E.SubElement(animation,'frame',tileid=str(i),duration='80')
        groups.setdefault('Animations / '+folder.parent.name,[]).append((folder.name,ids[0]+2,folder.relative_to(pack).as_posix()))
    for path in sorted((pack/'Other').glob('*.png'),key=natural):
        groups.setdefault('Other effects',[]).append((path.stem,tile(path)+2,path.relative_to(pack).as_posix()))
    collection.set('tilecount',str(len(nodes)))
    return split_collection(collection,out,groups,len(environment))


if __name__=='__main__':
    catalog=build(Path(__file__).resolve().parents[1])
    print(f'{len(catalog.sets)} fantasy category tilesets; {catalog.tile_count} tiles')
