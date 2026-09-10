"""Shared Tiled XML primitives for deterministic map generators (stdlib only)."""
from pathlib import Path
import struct
import xml.etree.ElementTree as E


def dimensions(path):
    with Path(path).open('rb') as image:
        image.seek(16)
        return struct.unpack('>II', image.read(8))


def write(node, path):
    E.indent(node)
    E.ElementTree(node).write(path, encoding='UTF-8', xml_declaration=True)


def drawing_offset(width, height, pivot, grid=(128, 64)):
    return round(grid[0] / 2 - width * pivot[0]), round(height * pivot[1] - grid[1] / 2)


def set_pivot(tileset, pivot, width=256, height=256):
    x, y = drawing_offset(width, height, pivot)
    E.SubElement(tileset, 'tileoffset', x=str(x), y=str(y))
    properties = E.SubElement(tileset, 'properties')
    E.SubElement(properties, 'property', name='tile_layer_alignment', value='bottom_left')
    E.SubElement(tileset, 'grid', orientation='isometric', width='128', height='64')


def image_tile(tileset, tile_id, source, image_path):
    width, height = dimensions(image_path)
    node = E.SubElement(tileset, 'tile', id=str(tile_id))
    E.SubElement(node, 'image', source=source, width=str(width), height=str(height))
    return node


def csv_layer(map_node, layer_id, name, width, height, data):
    if len(data) != width * height:
        raise ValueError('Tile data does not match layer dimensions')
    node = E.SubElement(map_node, 'layer', id=str(layer_id), name=name,
                        width=str(width), height=str(height))
    E.SubElement(node, 'data', encoding='csv').text = '\n' + ',\n'.join(
        ','.join(map(str, data[y * width:(y + 1) * width])) for y in range(height)) + '\n'
    return node
