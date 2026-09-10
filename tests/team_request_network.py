"""Check team request balancing and override on isolated servers."""
from build_identity import join_identity
import pathlib
import socket
import struct
import subprocess
import tempfile
import time
from world_transport import recv_world

root=pathlib.Path(__file__).resolve().parent.parent

def string(text):
    data=text.encode();return struct.pack('!I',len(data))+data

def receive(sock,kind):
    deadline=time.monotonic()+3
    while time.monotonic()<deadline:
        data=recv_world(sock);n=struct.unpack_from('!I',data)[0]
        if data[4:4+n]==kind.encode():return data[4+n:]
    raise AssertionError('No '+kind)

def team(sock,ident):
    data=receive(sock,'world');offset=0
    for i in range(ident+1):
        result=struct.unpack_from('!i',data,offset)[0];offset+=22
        n=struct.unpack_from('!I',data,offset)[0];offset+=4+n+31
        count=data[offset+4];offset+=5+count*20+50
    return result

for honor in (False,True):
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix='marpg-team-request-') as tmp:
        folder=pathlib.Path(tmp);(folder/'build').mkdir();(folder/'assets').symlink_to(root/'assets',target_is_directory=True)
        (folder/'server.toml').write_text(f'[server]\nip="127.0.0.1"\nport={port}\nslots=6\nteams=2\nbots=0\nhonor_team_requests={str(honor).lower()}\n')
        with open(folder/'server.log','w') as log:
            server=subprocess.Popen([str(root/'build/server')],cwd=folder/'build',stdout=log,stderr=log)
            sockets=[]
            try:
                deadline=time.monotonic()+15
                while 'Server listening on port' not in (folder/'server.log').read_text():
                    assert server.poll() is None and time.monotonic()<deadline
                    time.sleep(.02)
                for index,(request,expected) in enumerate([(2,1),(2,1 if honor else 0),(20,0)]):
                    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.settimeout(3);sockets.append(sock)
                    sock.sendto(string('join')+string(f'Request {index}')+struct.pack('!I',request)+join_identity[4:],('127.0.0.1',port))
                    reply=receive(sock,'join_ack');ident=struct.unpack_from('!i',reply)[0]
                    assert ident==index
                    offset=8  # Assigned id and protocol.
                    for _ in range(2):  # Build identity and configured server address.
                        length=struct.unpack_from('!I',reply,offset)[0];offset+=4+length
                    offset+=2+12+1+32  # Port, slots/teams/bots, friendly fire, damage settings.
                    assert bool(reply[offset])==honor
                    assert team(sock,ident)==expected,(honor,request,expected)
                # Retry from the same endpoint with a different preference must not move the player.
                sockets[0].sendto(string('join')+string('Request 0')+struct.pack('!I',1)+join_identity[4:],('127.0.0.1',port))
                assert struct.unpack_from('!i',receive(sockets[0],'join_ack'))[0]==0
                assert team(sockets[0],0)==1
            finally:
                for sock in sockets:sock.close()
                server.terminate();server.wait(timeout=5)
print('PASS: requested-team tie break, balance enforcement, unconditional override, unavailable team fallback, idempotent retries')
