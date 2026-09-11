#!/usr/bin/env python3
"""Refresh editor-only prefab previews; gameplay always loads the source prefabs."""
from pathlib import Path
import copy
import sys
import xml.etree.ElementTree as E
from map_authoring import write


def refresh(path):
    path = Path(path).resolve()
    root = E.parse(path).getroot()
    for group in list(root.findall('group')):
        if group.find("properties/property[@name='editor_only'][@value='true']") is not None:
            root.remove(group)
    tw, th = int(root.get('tilewidth')), int(root.get('tileheight'))
    refs = { (path.parent/r.get('source')).resolve(): int(r.get('firstgid')) for r in root.findall('tileset') }
    layer_id = max((int(n.get('id', 0)) for n in root.iter() if n.tag in ('layer','objectgroup','group')), default=0) + 1
    object_id = max((int(n.get('id', 0)) for n in root.iter('object')), default=0) + 1
    for marker in root.findall("objectgroup[@name='Prefabs']/object"):
        prop = marker.find("properties/property[@name='prefab']")
        if prop is None:
            raise ValueError('Prefab marker missing prefab file property')
        source = (path.parent/prop.get('value')).resolve()
        prefab = E.parse(source).getroot()
        if (int(prefab.get('tilewidth')),int(prefab.get('tileheight'))) != (tw,th):
            raise ValueError('Prefab grid must match host map')
        mapping = {}
        for ref in prefab.findall('tileset'):
            tsx = (source.parent/ref.get('source')).resolve()
            if tsx not in refs:
                raise ValueError(f'Import {tsx.name} into the host map before refreshing previews')
            ts = E.parse(tsx).getroot()
            count = max(int(ts.get('tilecount','0')), max((int(t.get('id'))+1 for t in ts.findall('tile')),default=0))
            mapping.update({int(ref.get('firstgid'))+i: refs[tsx]+i for i in range(count)})
        def gid(value):
            value=int(value)
            return (mapping[value & 0x0fffffff] | (value & 0xf0000000)) if value else 0
        x,y=float(marker.get('x')),float(marker.get('y'))
        group = E.SubElement(root,'group',id=str(layer_id),name='Preview: '+marker.get('name',source.stem),
                             offsetx=str((x-y)*tw/(2*th)),offsety=str((x+y)/2),locked='1')
        layer_id += 1
        props=E.SubElement(group,'properties')
        E.SubElement(props,'property',name='editor_only',type='bool',value='true')
        for original in prefab:
            if original.tag not in ('layer','objectgroup'):
                continue
            layer=copy.deepcopy(original); layer.set('id',str(layer_id));layer_id+=1
            if layer.tag=='layer':
                data=layer.find('data')
                if data.get('encoding')!='csv': raise ValueError('Preview requires CSV tile layers')
                data.text=','.join(str(gid(v)) for v in data.text.split(',') if v.strip())
            for obj in layer.findall('object'):
                obj.set('id',str(object_id));object_id+=1
                if obj.get('gid'):obj.set('gid',str(gid(obj.get('gid'))))
            group.append(layer)
    root.set('nextlayerid',str(layer_id));root.set('nextobjectid',str(object_id))
    write(root,path)

if __name__=='__main__':
    if len(sys.argv)!=2: raise SystemExit('Usage: python3 tools/refresh_prefab_previews.py assets/tiled/city.tmx')
    refresh(sys.argv[1])
