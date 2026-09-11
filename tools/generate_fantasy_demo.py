#!/usr/bin/env python3
"""Build the complete fantasy-pack Tiled gallery (stdlib only; originals unchanged)."""
from pathlib import Path
import xml.etree.ElementTree as E
import math, re, json
from map_authoring import dimensions, write, drawing_offset, set_pivot, image_tile, csv_layer
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'assets/Fantasy tileset - 2D Isometric'
OUT=ROOT/'assets/tiled'
META=OUT/'gallery';META.mkdir(exist_ok=True)
from fantasy_tileset import build, natural
catalog=build(ROOT)
groups=catalog.groups;environment_count=catalog.environment_count
entries=[];sets=list(catalog.sets);next_gid=catalog.tile_count+1
floor_gid=catalog.lookup['Environment/Ground A1_E.png']
for path in sorted((PACK/'Characters').glob('*/*.png'),key=natural):
    w,h=dimensions(path);assert (w,h)==(1920,1024),path
    name=path.parent.name+' - '+path.stem;filename=re.sub(r'[^A-Za-z0-9_-]','_',name)+'.tsx'
    ts=E.Element('tileset',version='1.10',name=name,tilewidth='128',tileheight='128',tilecount='120',columns='15')
    E.SubElement(ts,'image',source='../../Fantasy tileset - 2D Isometric/'+path.relative_to(PACK).as_posix(),width=str(w),height=str(h))
    tiles=[]
    for frame in range(120):
        node=E.SubElement(ts,'tile',id=str(frame));tiles.append(node)
        properties=E.SubElement(node,'properties')
        E.SubElement(properties,'property',name='player_animation',type='bool',value='true')
    animation=E.SubElement(tiles[0],'animation')
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
for lid,name,data in [(2,'Floor',[floor_gid]*(WIDTH*HEIGHT)),(3,'Walls',[0]*(WIDTH*HEIGHT))]:
    if lid==3:
        for x,y,gid in placements:data[y*WIDTH+x]=gid
    csv_layer(m,lid,name,WIDTH,HEIGHT,data)
spawns=E.SubElement(m,'objectgroup',id='4',name='Spawns')
for i in range(20):
    x=4+i%10*2;y=5+i//10*2
    node=E.SubElement(spawns,'object',id=str(len(labels)+1+i),name='Gallery spawn '+str(i+1),x=str(x*64+64),y=str(y*64));E.SubElement(node,'point')
write(m,OUT/'demo.tmx');(META/'manifest.json').write_text(json.dumps(dict(exhibits=entries,environment_images=environment_count,character_sheets=len(sets)-len(catalog.sets),animation_sequences=sum(len(v) for k,v in groups.items() if k.startswith('Animations')),width=WIDTH,height=HEIGHT),indent=2)+'\n')
print(f'{len(entries)} exhibits; {catalog.tile_count} collection frames; {WIDTH}x{HEIGHT} map')
