"""A client that loads for seven seconds must retain its slot without input."""
import pathlib, socket, subprocess, tempfile, time
root=pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as directory:
    path=pathlib.Path(directory);(path/'build').mkdir();(path/'assets').symlink_to(root/'assets',target_is_directory=True)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    (path/'server.toml').write_text(f'[server]\nip="127.0.0.1"\nport={port}\nslots=1\nteams=1\nbots=0\nmap="arena"\n')
    with (path/'server.log').open('w') as log:
        server=subprocess.Popen([str(root/'build/server')],cwd=path/'build',stdout=log,stderr=log)
        try:
            deadline=time.monotonic()+10
            while 'Server listening' not in (path/'server.log').read_text():
                assert server.poll() is None and time.monotonic()<deadline,(path/'server.log').read_text()
                time.sleep(.05)
            subprocess.run([str(root/'build/connection_probe'),str(port),'loading'],cwd=root/'build',check=True,timeout=15)
        finally:
            server.terminate();server.wait(timeout=5)
