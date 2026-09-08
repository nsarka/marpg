"""Run all UDP regression tests on isolated configured servers."""
import os
import pathlib
import socket
import subprocess
import tempfile
import time

root=pathlib.Path(__file__).resolve().parent.parent
for test in ['join_network.py','disconnect_network.py','combat_network.py','reliable_attack_network.py','collision_network.py']:
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix='marpg-network-') as tmp:
        test_build=pathlib.Path(tmp)/'build';test_build.mkdir()
        (pathlib.Path(tmp)/'assets').symlink_to(root/'assets',target_is_directory=True)
        config=pathlib.Path(tmp)/'server.toml'
        config.write_text((root/'tests/network.toml').read_text().replace('port = 54000',f'port = {port}'))
        with open(pathlib.Path(tmp)/'server.log','w') as log:
            server=subprocess.Popen([str(root/'build/server')],cwd=test_build,stdout=log,stderr=log)
            try:
                deadline=time.monotonic()+15
                while 'Server listening on port' not in (pathlib.Path(tmp)/'server.log').read_text():
                    assert server.poll() is None,'Test server failed to start'
                    assert time.monotonic()<deadline,'Test server did not become ready'
                    time.sleep(.02)
                subprocess.run(['python3',str(root/'tests'/test)],env={**os.environ,'MARPG_TEST_PORT':str(port)},check=True,timeout=30)
            finally:
                server.terminate();server.wait(timeout=5)
