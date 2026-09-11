"""Ground-footprint profiles for the fantasy environment collection.

Coordinates are in the source image, not its opaque sprite silhouette: canopies,
shadows and raised artwork must not become ground-level barriers. Profiles are
convex; corners use multiple polygons. Edit these rules before regenerating.
"""
import re
import xml.etree.ElementTree as E
from pathlib import PurePosixPath


def diamond(rx=24, ry=12, cx=128, cy=210):
    return [(cx-rx, cy), (cx, cy-ry), (cx+rx, cy), (cx, cy+ry)]


def rotate(points, direction):
    # Rotate on the ground plane, then project back to the 2:1 isometric image.
    result=[]
    for x,y in points:
        u=(x-128)/2+(y-210); v=(y-210)-(x-128)/2
        for _ in range('ESWN'.index(direction)):
            u,v=-v,u
        result.append((128+u-v,210+(u+v)/2))
    return result


def footprints(source):
    path=PurePosixPath(source)
    name=path.stem
    if source.startswith('Animations/'):
        group=path.parent.name
        if group.startswith('WindMill'):
            return [diamond(40,20,256,420)]
        if group.startswith('Barrel '):
            return [diamond(18,9)]
        if group in ('Turret1','Torch 1'):
            return [diamond(20 if group=='Turret1' else 5,10 if group=='Turret1' else 3)]
        return [] # Portals, water, fire, gas and destruction effects are passable.
    if not source.startswith('Environment/'):
        return []
    match=re.fullmatch(r'(\w+) ([A-Z])(\d+)_([ENSW])',name)
    if not match:
        if name in ('Torch','Torch2','Torch East','Torch West'): return [diamond(5,3)]
        if name in ('FirePlace','TreeTrunkBroken'): return [diamond(22,11)]
        return []
    category,family,number,direction=match.groups(); n=int(number)
    full=diamond(64,32)
    edge=[(112,228),(128,236),(192,204),(176,196)]
    shapes=[]
    if category=='Tree': shapes=[full] # Dense forest tiles form continuous barriers.
    elif category=='Chest': shapes=[diamond(26,13)]
    elif category=='Stone':
        if n not in (6,7,8): shapes=[diamond(30 if n in (1,2,9,10,12) else 17,15 if n in (1,2,9,10,12) else 9)]
        # Circular portal platforms are deliberately walkable.
    elif category=='Roof': shapes=[full]
    elif category=='Door':
        # Open/retracted doors do not fill the doorway.
        if family=='A' and n==1 or family=='C' and n in (1,2,5): shapes=[edge]
    elif category=='Wall':
        if family=='A' and n in (12,13,14,22): return [] # Steps/platforms.
        if (family in ('C','D','F') and n in (5,6)) or (family=='G' and n==10): return [] # Raised lintels.
        if family=='E' and n in (7,10,11,12): return [] # Floating horizontal beams/braces.
        if family=='B' and n in (7,8): shapes=[[(64,230),(72,234),(192,174),(184,170)]]
        elif (family in ('A','B','C','D','F') and n==2) or (family=='G' and n in (3,11)):
            shapes=[edge,rotate(edge,'N')]
        elif (family=='A' and n in (6,19)) or (family=='B' and n==10) or (family=='E' and n<=6): shapes=[diamond(9,5)]
        elif family=='A' and n in (8,21): shapes=[diamond(42,21)]
        else: shapes=[edge]
    elif category=='Ground':
        if family=='G' and n in (12,13,14,15,18,19,20,21,22): shapes=[edge] # Cliff faces.
        elif family=='D' and n in (2,3,4): shapes=[edge]
    elif category=='Misc':
        if family=='A':
            if n!=1: shapes=[diamond(12 if n in (2,7,8) else 26,6 if n in (2,7,8) else 13)]
        elif family=='B':
            if n in (13,23,25,29,30,31,32,33,34,35,36,37,38,39,40,59): return [] # Mats, ramps, platforms, hanging signs/canopies.
            if n in (24,44,45,52): shapes=[diamond(5,3)]
            elif n in (3,5,6,10,12,14,17,18,19,41,42,43,46,47,49,50,55,56,58): shapes=[diamond(42,21)]
            else: shapes=[diamond(22,11)]
        elif family=='C':
            if n==14: # Portal arch: two pillars leave the opening usable.
                shapes=[diamond(7,4,94,227),diamond(7,4,162,193)]
            else: shapes=[diamond(24,12)]
        elif family=='D': shapes=[diamond(5,3)]
        elif family=='E' and n<=4: shapes=[diamond(26,13)] # Hay bales; crops remain walkable.
    return [rotate(shape,direction) for shape in shapes]


def apply_collisions(tile, source):
    shapes=footprints(source)
    props=tile.find('properties')
    if props is None: props=E.SubElement(tile,'properties')
    E.SubElement(props,'property',name='collision_profile',value='solid' if shapes else 'passable')
    if not shapes: return
    group=E.SubElement(tile,'objectgroup',draworder='index')
    for i,points in enumerate(shapes,1):
        obj=E.SubElement(group,'object',id=str(i),name='Solid footprint',x='0',y='0')
        E.SubElement(obj,'polygon',points=' '.join(f'{x:g},{y:g}' for x,y in points))
