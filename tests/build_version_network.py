"""Check build identity rejection before slot allocation, using an isolated server."""
import pathlib, re, socket, struct, subprocess, tempfile, time
root = pathlib.Path(__file__).resolve().parent.parent
commit = re.search(r'"([0-9a-f]{40})"', (root/'build/generated/build_version.hpp').read_text())[1]
def string(value):
    data=value.encode()
    return struct.pack('!I',len(data))+data
def read_string(data):
    size=struct.unpack_from('!I',data)[0]
    return data[4:4+size].decode(),data[4+size:]
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock, tempfile.TemporaryDirectory() as directory:
    sock.bind(('127.0.0.1',0));sock.settimeout(.25)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    path=pathlib.Path(directory);(path/'build').mkdir()
    (path/'assets').symlink_to(root/'assets',target_is_directory=True)
    (path/'server.toml').write_text(f'''[server]
ip="127.0.0.1"
port={port}
slots=1
teams=1
bots=0
''')
    with open(path/'server.log','w') as log:
        process=subprocess.Popen([str(root/'build/server')],cwd=path/'build',stdout=log,stderr=log)
        try:
            time.sleep(.5)
            assert process.poll() is None,(path/'server.log').read_text()
            for requested,expected in [('0'*40,'version_mismatch'),(commit,'join_ack')]:
                deadline=time.monotonic()+10
                while True:
                    sock.sendto(string('join')+string('Version test')+struct.pack('!I',0)+string(requested),('127.0.0.1',port))
                    try:
                        data,_=sock.recvfrom(65535);kind,body=read_string(data)
                        if kind in ('version_mismatch','join_ack'):break
                    except socket.timeout:
                        assert time.monotonic()<deadline,(path/'server.log').read_text()
                assert kind==expected,(kind,expected)
                if kind=='version_mismatch':
                    assert read_string(body)[0]==commit
                else:
                    assigned,protocol=struct.unpack_from('!iI',body)
                    assert assigned==0 and protocol==7
                    assert read_string(body[8:])[0]==commit
            print('PASS: mismatched build rejected without consuming slot; matching build accepted with server hash')
        finally:
            process.terminate()
            process.wait(timeout=5)
