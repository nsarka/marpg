"""Add/update the city's seven appearance buttons without regenerating other districts."""
from pathlib import Path
import sys
import xml.etree.ElementTree as E
from map_authoring import write

NAMES = ('Enemy 1', 'Enemy 2', 'Enemy 3', 'NPC1', 'NPC2', 'NPC3', 'Player')
CLASSES = ('Adventurer', 'Bruiser', 'Caster', 'Charger')


def add_character_select_area(city, tiled_directory):
    width = int(city.get('width'))
    gids = {}
    for ref in city.findall('tileset'):
        ts = E.parse(Path(tiled_directory) / ref.get('source'))
        for tile in ts.findall('tile'):
            image = tile.find('image')
            if image is not None:
                gids[Path(image.get('source')).stem] = int(ref.get('firstgid')) + int(tile.get('id'))
    # An open court east of the castle, plus an L-shaped connection to the avenue.
    cells = {(x, y) for x in range(26, 45) for y in range(3, 21)}
    cells |= {(x, y) for x in range(26, 29) for y in range(20, 28)}
    for layer in city.findall('layer'):
        name = layer.get('name')
        data = layer.find('data')
        if data is None or data.get('encoding') != 'csv':
            continue
        values = [int(v) for v in data.text.split(',') if v.strip()]
        for x, y in cells:
            values[y * width + x] = (gids['Ground A1_E'] if name == 'Floor' else
                                      gids['Ground H1_E'] if name == 'Paving' else 0)
        data.text = '\n' + ',\n'.join(','.join(map(str, values[y * width:(y + 1) * width]))
                                     for y in range(int(city.get('height')))) + '\n'
    triggers = city.find("objectgroup[@name='Triggers']")
    labels = city.find("objectgroup[@name='Labels']")
    for group in (triggers, labels):
        for obj in list(group.findall('object')):
            if obj.get('type') in ('CharacterSelect', 'ClassSelect') or obj.get('name') in ('Character Selection', 'Class Selection'):
                group.remove(obj)
    next_id = max(int(o.get('id', '0')) for o in city.findall('.//object')) + 1
    for index, name in enumerate(NAMES):
        x, y = 29 + (index % 4) * 3, 11 + (index // 4) * 4
        cx, cy = (x + .5) * 64, (y + .5) * 64
        obj = E.SubElement(triggers, 'object', id=str(next_id), name=name, type='CharacterSelect',
                           x=str(cx - 44), y=str(cy - 44), width='88', height='88')
        props = E.SubElement(obj, 'properties')
        E.SubElement(props, 'property', name='character', type='int', value=str(index))
        next_id += 1
    # Labels use the renderer's original isometric projection (without the trigger half-tile shift).
    E.SubElement(labels, 'object', id=str(next_id), name='Character Selection',
                 x=str(26 * 64 + 32), y=str(8 * 64 - 32))
    next_id += 1
    for index, name in enumerate(CLASSES):
        x, y = 29 + index * 3, 7
        cx, cy = (x + .5) * 64, (y + .5) * 64
        obj = E.SubElement(triggers, 'object', id=str(next_id), name=name, type='ClassSelect',
                           x=str(cx - 44), y=str(cy - 44), width='88', height='88')
        props = E.SubElement(obj, 'properties')
        E.SubElement(props, 'property', name='class', type='int', value=str(index))
        next_id += 1
    E.SubElement(labels, 'object', id=str(next_id), name='Class Selection',
                 x=str(31 * 64 + 32), y=str(4 * 64 - 32))
    city.set('nextobjectid', str(next_id + 1))


if __name__ == '__main__':
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'assets/tiled/city.tmx'
    city = E.parse(path).getroot()
    add_character_select_area(city, path.parent)
    write(city, path)
    print('Added seven appearance buttons and four class buttons beside the castle')
