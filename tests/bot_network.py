"""Run a ten-bot battle on an isolated server and observe authoritative updates."""
import pathlib
import socket
import struct
import subprocess
import tempfile
import time
import math
from world_transport import recv_world
root=pathlib.Path(__file__).resolve().parent.parent

def string(text):
    data=text.encode();return struct.pack('!I',len(data))+data

def decode(data):
    n=struct.unpack_from('!I',data)[0]
    if data[4:4+n]!=b'world':return None
    offset=4+n;players=[]
    for _ in range(32):
        team=struct.unpack_from('!i',data,offset)[0];offset+=4
        connected,alive=data[offset:offset+2];pos=struct.unpack_from('=ff',data,offset+2);offset+=18
        size=struct.unpack_from('!I',data,offset)[0];offset+=4+size
        health=struct.unpack_from('!i',data,offset)[0];offset+=8
        attack,sequence=struct.unpack_from('!BI',data,offset);offset+=23
        count=data[offset+4];offset+=5;events=[]
        for _ in range(count):
            serial,amount,source=struct.unpack_from('!Iii',data,offset);offset+=20
            events.append((serial,amount,source))
        players.append((connected,alive,pos,team,health,attack,sequence,events))
    return players

with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
    reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
with tempfile.TemporaryDirectory(prefix='marpg-bots-') as tmp:
    folder=pathlib.Path(tmp);(folder/'build').mkdir();(folder/'assets').symlink_to(root/'assets',target_is_directory=True)
    (folder/'server.toml').write_text(f'[server]\nip="127.0.0.1"\nport={port}\nplayers=20\nteams=2\nbots=10\n')
    with open(folder/'server.log','w') as log:
        server=subprocess.Popen([str(root/'build/server')],cwd=folder/'build',stdout=log,stderr=log)
        observer=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);observer.settimeout(3)
        try:
            deadline=time.monotonic()+15
            while 'Server listening on port' not in (folder/'server.log').read_text():
                assert server.poll() is None
                assert time.monotonic()<deadline,'Bot server did not become ready'
                time.sleep(.02)
            observer.sendto(string('join')+string('AI observer'),('127.0.0.1',port))
            while True:
                data=recv_world(observer);n=struct.unpack_from('!I',data)[0]
                if data[4:4+n]==b'join_ack':ident=struct.unpack_from('!i',data,4+n)[0];break
            assert ident==10,ident
            initial=None;moved=set();attacks=set();damaged=set();kinds=set();seq=0;snapshots=0
            deadline=time.monotonic()+18
            while time.monotonic()<deadline:
                observer.sendto(string('state')+struct.pack('!II',ident,seq)+struct.pack('=ff',0,0)+bytes([1,0,0,0,0,0,0])+struct.pack('=ff',1,0),('127.0.0.1',port));seq+=1
                players=decode(recv_world(observer))
                if not players:continue
                snapshots+=1
                if initial is None:initial=players
                for i,p in enumerate(players[:10]):
                    assert p[0],f'Bot {i} disconnected'
                    if math.dist(p[2],initial[i][2])>10:moved.add(i)
                    if p[6]:attacks.add(i);kinds.add(p[5])
                    for serial,amount,source in p[7]:
                        assert source>=0,'Bot walked into an environmental hazard'
                        assert players[source][3]!=p[3],'Bot damaged its teammate'
                        damaged.add(i)
            assert len(moved)==10,('Bots not moving',moved)
            assert len(attacks)>=6,('Too few bots reached combat',attacks)
            assert len(damaged)>=4,('No sustained enemy combat',damaged)
            assert {1,2}.issubset(kinds),kinds
            assert snapshots>250,('Server update rate too slow',snapshots)
            print(f'PASS: all 10 bots moved; {len(attacks)} attacked; {len(damaged)} took enemy hits; both attacks observed; {snapshots} snapshots')
        finally:
            observer.close();server.terminate();server.wait(timeout=5)
