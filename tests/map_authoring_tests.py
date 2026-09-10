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
    for name in ['map_authoring.py', 'generate_farms.py', 'generate_fantasy_demo.py']:
        shutil.copy(root / 'tools' / name, project / 'tools')
    for name in ['generate_farms.py', 'generate_fantasy_demo.py']:
        subprocess.run([sys.executable, str(project / 'tools' / name)], check=True)
    for path in (project / 'assets/tiled').glob('farms*'):
        assert path.read_bytes() == (root / 'assets/tiled' / path.name).read_bytes(), path.name
    manifest = json.loads((project / 'assets/tiled/gallery/manifest.json').read_text())
    assert len(manifest['exhibits']) == 2108
    for path in (project / 'assets/tiled').rglob('*.tsx'):
        for image in E.parse(path).findall('.//image'):
            assert (path.parent / image.get('source')).is_file(), image.get('source')
    print('PASS: identical farms output, complete gallery inventory, all image references resolve')
