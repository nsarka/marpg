from build_identity import join_identity
"""Run against a fresh local server."""
import os
from world_transport import recv_world
import socket
import struct
import time

SERVER = ('127.0.0.1', int(os.environ.get('MARPG_TEST_PORT','54000')))

def string(value):
    data = value.encode()
    return struct.pack('!I', len(data)) + data

def message(sock, kind, suffix=b''):
    sock.sendto(string(kind) + suffix, SERVER)

def receive(sock, kind, timeout=2):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        sock.settimeout(max(.001,deadline-time.monotonic()))
        data=recv_world(sock)
        size=struct.unpack_from('!I',data)[0]
        if data[4:4+size].decode()==kind:
            return data[4+size:]
    raise AssertionError('Missing '+kind)

def join(sock,name):
    message(sock,'join',string(name)+join_identity)
    return struct.unpack_from('!i',receive(sock,'join_ack'))[0]

def states(sock):
    data=receive(sock,'world');offset=0;result=[]
    for _ in range(32):
        offset+=4
        connected,alive=struct.unpack_from('??',data,offset)
        offset+=18
        size=struct.unpack_from('!I',data,offset)[0];offset+=4
        name=data[offset:offset+size].decode();offset+=size
        health=struct.unpack_from('!i',data,offset)[0];offset+=31
        count=data[offset+4];offset+=5+count*20
        result.append((connected,alive,name,health))
    return result

def wait_state(sock,id,connected,name=None,timeout=2):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        state=states(sock)[id]
        if state[0]==connected and (name is None or state[2]==name): return state
    raise AssertionError('World did not publish expected membership')

sockets=[socket.socket(socket.AF_INET,socket.SOCK_DGRAM) for _ in range(4)]
try:
    observer,victim,replacement,attacker=sockets
    observer_id=join(observer,'Observer');victim_id=join(victim,'Victim')
    wait_state(observer,victim_id,True)
    message(attacker,'leave',struct.pack('!I',victim_id))
    assert wait_state(observer,victim_id,True)[0], 'Forged leave disconnected victim'
    for _ in range(2):
        message(victim,'leave',struct.pack('!I',victim_id))
        assert struct.unpack('!I',receive(victim,'leave_ack'))[0]==victim_id
    wait_state(observer,victim_id,False)
    assert join(replacement,'Replacement')==victim_id
    state=wait_state(observer,victim_id,True,'Replacement')
    assert state[1] and state[3]==100, state
    replacement.close()  # No leave packet: simulate killed process.
    deadline=time.monotonic()+6
    while time.monotonic()<deadline:
        assert join(observer,'Observer')==observer_id  # Keep observer alive.
        time.sleep(.3)
    wait_state(observer,victim_id,False)
    assert join(attacker,'After timeout')==victim_id
    assert wait_state(observer,0,True)[0], 'Bots must not time out'
    print('PASS: acknowledged and duplicate leave, endpoint checks, membership snapshots, clean slot reuse, crash timeout, bots')
finally:
    for sock in sockets: sock.close()
