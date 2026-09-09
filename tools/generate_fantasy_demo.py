#!/usr/bin/env python3
"""Build the complete fantasy-pack Tiled gallery (stdlib only; originals unchanged)."""
from pathlib import Path
import xml.etree.ElementTree as E
import struct, math, re, json
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'assets/Fantasy tileset - 2D Isometric'
OUT=ROOT/'assets/tiled'
META=OUT/'gallery';META.mkdir(exist_ok=True)
def dimensions(path):
    with path.open('rb') as f:f.seek(16);return struct.unpack('>II',f.read(8))
def write(node,path):
    E.indent(node);E.ElementTree(node).write(path,encoding='UTF-8',xml_declaration=True)
def natural(path):return [int(v) if v.isdigit() else v.lower() for v in re.split(r'(\d+)',str(path))]
collection=E.Element('tileset',version='1.10',tiledversion='1.12.2',name='Fantasy pack gallery',tilewidth='256',tileheight='256',columns='0')
E.SubElement(collection,'tileoffset',x='0',y='12')
entries=[];tile_nodes=[];groups={}
def tile(path):
    i=len(tile_nodes);w,h=dimensions(path)
    node=E.SubElement(collection,'tile',id=str(i));E.SubElement(node,'image',source='../Fantasy tileset - 2D Isometric/'+path.relative_to(PACK).as_posix(),width=str(w),height=str(h));tile_nodes.append(node)
    return i
# The base floor lives in its own small tileset, so the floor layer never loads the gallery textures.
environment=sorted((PACK/'Environment').glob('*.png'),key=natural)
for path in environment:
    category=path.stem.split()[0];category='Other environment' if category.startswith('Shadow') or category in ('TilePreview','BLACK','Torch2','FirePlace','TreeTrunkBroken') else category
    groups.setdefault('Environment / '+category,[]).append((path.stem,tile(path)+2,path.relative_to(PACK).as_posix()))
for folder in sorted((PACK/'Animations').glob('*/*'),key=natural):
    frames=sorted(folder.glob('*.png'),key=natural)
    if not frames:continue
    indices=[tile(p) for p in frames];animation=E.SubElement(tile_nodes[indices[0]],'animation')
    for index in indices:E.SubElement(animation,'frame',tileid=str(index),duration='80')
    groups.setdefault('Animations / '+folder.parent.name,[]).append((folder.name,indices[0]+2,folder.relative_to(PACK).as_posix()))
for path in sorted((PACK/'Other').glob('*.png'),key=natural):groups.setdefault('Other effects',[]).append((path.stem,tile(path)+2,path.relative_to(PACK).as_posix()))
collection.set('tilecount',str(len(tile_nodes)));write(collection,OUT/'pack_gallery.tsx')
base=E.Element('tileset',version='1.10',name='Gallery floor',tilewidth='256',tileheight='256',tilecount='1',columns='0');E.SubElement(base,'tileoffset',x='0',y='12');node=E.SubElement(base,'tile',id='0');E.SubElement(node,'image',source='../Fantasy tileset - 2D Isometric/Environment/Ground A1_E.png',width='256',height='256');write(base,OUT/'gallery_floor.tsx')
sets=[(1,'gallery_floor.tsx'),(2,'pack_gallery.tsx')];next_gid=len(tile_nodes)+2
for path in sorted((PACK/'Characters').glob('*/*.png'),key=natural):
    w,h=dimensions(path);assert (w,h)==(1920,1024),path
    name=path.parent.name+' - '+path.stem;filename=re.sub(r'[^A-Za-z0-9_-]','_',name)+'.tsx'
    ts=E.Element('tileset',version='1.10',name=name,tilewidth='128',tileheight='128',tilecount='120',columns='15')
    E.SubElement(ts,'image',source='../../Fantasy tileset - 2D Isometric/'+path.relative_to(PACK).as_posix(),width=str(w),height=str(h))
    animation=E.SubElement(E.SubElement(ts,'tile',id='0'),'animation')
    for frame in range(120):E.SubElement(animation,'frame',tileid=str(frame),duration='80')
    write(ts,META/filename);sets.append((next_gid,'gallery/'+filename));groups.setdefault('Characters / '+path.parent.name,[]).append((path.stem+' (8 directions)',next_gid,path.relative_to(PACK).as_posix()));next_gid+=120
WIDTH=128;row=10;placements=[];labels=[('FANTASY PACK — COMPLETE GALLERY',5,2),('Walk through labeled exhibits. Animated displays loop; character sheets cycle all 8 directions.',5,3)]
for category,items in groups.items():
    labels.append((category.upper(),3,row));row+=3
    for n,(name,gid,source) in enumerate(items):
        x=4+(n%40)*3;y=row+(n//40)*3
        placements.append((x,y,gid));labels.append((name,x,y+1));entries.append(dict(category=category,name=name,source=source,gid=gid,x=x,y=y))
    row+=math.ceil(len(items)/40)*3+3
HEIGHT=row+4
m=E.Element('map',version='1.10',tiledversion='1.12.2',orientation='isometric',renderorder='right-down',width=str(WIDTH),height=str(HEIGHT),tilewidth='128',tileheight='64',infinite='0',nextlayerid='5',nextobjectid=str(len(labels)+21))
for gid,source in sets:E.SubElement(m,'tileset',firstgid=str(gid),source=source)
label_group=E.SubElement(m,'objectgroup',id='1',name='Labels')
for i,(text,x,y) in enumerate(labels,1):E.SubElement(label_group,'object',id=str(i),name=text,x=str(x*64+64),y=str(y*64))
for lid,name,data in [(2,'Floor',[1]*(WIDTH*HEIGHT)),(3,'Walls',[0]*(WIDTH*HEIGHT))]:
    if lid==3:
        for x,y,gid in placements:data[y*WIDTH+x]=gid
    layer=E.SubElement(m,'layer',id=str(lid),name=name,width=str(WIDTH),height=str(HEIGHT));E.SubElement(layer,'data',encoding='csv').text='\n'+',\n'.join(','.join(map(str,data[y*WIDTH:(y+1)*WIDTH])) for y in range(HEIGHT))+'\n'
spawns=E.SubElement(m,'objectgroup',id='4',name='Spawns')
for i in range(20):
    x=4+i%10*2;y=5+i//10*2
    node=E.SubElement(spawns,'object',id=str(len(labels)+1+i),name='Gallery spawn '+str(i+1),x=str(x*64+64),y=str(y*64));E.SubElement(node,'point')
write(m,OUT/'demo.tmx');(META/'manifest.json').write_text(json.dumps(dict(exhibits=entries,environment_images=len(environment),character_sheets=len(sets)-2,animation_sequences=sum(len(v) for k,v in groups.items() if k.startswith('Animations')),width=WIDTH,height=HEIGHT),indent=2)+'\n')
print(f'{len(entries)} exhibits; {len(tile_nodes)} collection frames; {WIDTH}x{HEIGHT} map')
