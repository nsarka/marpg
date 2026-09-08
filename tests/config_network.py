"""Self-contained configured-server integration test; does not stop a running game."""
import pathlib
import socket
import struct
import subprocess
import tempfile
import time
from world_transport import recv_world

root=pathlib.Path(__file__).resolve().parent.parent

def string(value):
    data=value.encode();return struct.pack('!I',len(data))+data

def receive(sock,kind):
    deadline=time.monotonic()+2
    while time.monotonic()<deadline:
        data=recv_world(sock);size=struct.unpack_from('!I',data)[0]
        if data[4:4+size]==kind.encode():return data[4+size:]
    raise AssertionError('No '+kind)

def states(sock):
    data=receive(sock,'world');offset=0;result=[]
    for _ in range(32):
        team=struct.unpack_from('!i',data,offset)[0];offset+=4
        connected=data[offset];pos=struct.unpack_from('=ff',data,offset+2);offset+=18
        n=struct.unpack_from('!I',data,offset)[0];offset+=4
        name=data[offset:offset+n].decode();offset+=n
        health=struct.unpack_from('!i',data,offset)[0];offset+=31
        count=data[offset+4];offset+=5+count*20
        result.append((connected,team,pos,name,health))
    return result

with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
    reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
with tempfile.TemporaryDirectory(prefix='marpg-config-') as tmp:
    test_build=pathlib.Path(tmp)/'build';test_build.mkdir()
    (pathlib.Path(tmp)/'assets').symlink_to(root/'assets',target_is_directory=True)
    config=pathlib.Path(tmp)/'server.toml'
    config.write_text(f'''[server]
ip = "127.0.0.1"
port = {port}
players = 6
teams = 3
bots = 0
[damage]
trigger = 7
trigger_bpm = 60
out_of_bounds = 9
out_of_bounds_bpm = 240
jab = 27
hook = 41
''')
    sockets=[]
    with open(pathlib.Path(tmp)/'server.log','w') as log:
        process=subprocess.Popen([str(root/'build/server')],cwd=test_build,stdout=log,stderr=log)
        try:
            time.sleep(.3);assert process.poll() is None,'Server failed to bind configured address'
            subprocess.run([str(root/'build/connection_probe'),str(port)],cwd=root/'build',check=True,timeout=12)
            for i in range(7):
                s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.settimeout(2);sockets.append(s)
                s.sendto(string('join')+string(f'Player {i}'),('127.0.0.1',port))
                ack=receive(s,'join_ack');ident=struct.unpack_from('!i',ack)[0]
                assert ident==(i if i<6 else -1),(i,ident)
                assert struct.unpack_from('!I',ack,4)[0]==2
                if i<6:
                    current=states(s)
                    assert current[i][0] and current[i][1]==i%3 and current[i][3]==f'Player {i}',current[i]
            current=states(sockets[5]);assert [sum(p[0] and p[1]==t for p in current) for t in range(3)]==[2,2,2]
            # Reclaim a red slot and ensure the replacement joins the now-smaller red team.
            sockets[0].sendto(string('leave')+struct.pack('!I',0),('127.0.0.1',port));receive(sockets[0],'leave_ack')
            newcomer=sockets[6];newcomer.sendto(string('join')+string('Replacement'),('127.0.0.1',port))
            assert struct.unpack_from('!i',receive(newcomer,'join_ack'))[0]==0
            deadline=time.monotonic()+2
            while True:
                updated=states(newcomer)
                if updated[0][3]=='Replacement':break
                assert time.monotonic()<deadline
            assert updated[0][1]==0 and updated[0][4]==100
            print('PASS: custom IP/port, synchronized settings, player cap, balanced teams, team assignment on slot reuse')
        finally:
            for s in sockets:s.close()
            process.terminate();process.wait(timeout=5)
