from build_identity import join_identity
"""Run against a fresh demo server: python3 tests/player_collision_network.py."""
import os
from world_transport import recv_world
import socket, struct, time, math
server = ('127.0.0.1', int(os.environ.get('MARPG_TEST_PORT','54000')))
def string(s):
    b=s.encode(); return struct.pack('!I',len(b))+b

def connect(name):
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(2)
    s.sendto(string('join')+string(name)+join_identity,server)
    while True:
        b=recv_world(s); n=struct.unpack_from('!I',b)[0]
        if b[4:4+n]==b'join_ack':
            ident=struct.unpack_from('!i',b,4+n)[0]; assert ident>=0
            return s,ident

a,aid=connect('Collision A'); b,bid=connect('Collision B')

def positions(data):
    offset=4+struct.unpack_from('!I',data)[0]
    if data[4:offset]!=b'world': return None
    result=[]
    for i in range(32):
        offset+=4
        result.append(struct.unpack_from('=ff',data,offset+2) if data[offset] else None); offset+=18
        n=struct.unpack_from('!I',data,offset)[0]; offset+=4+n+31
        count=data[offset+4];offset+=5+count*20
    return result

initial=None
while initial is None or initial[aid] is None or initial[bid] is None:
    initial=positions(recv_world(a))
delta=(initial[bid][0]-initial[aid][0],initial[bid][1]-initial[aid][1])
length=math.hypot(*delta);direction=(delta[0]/length,delta[1]/length)
start=time.monotonic(); next_send=0; seq=0; latest=None; contact=False
while time.monotonic()-start<1.5:
    now=time.monotonic()
    if now>=next_send:
        for s,ident,x in [(a,aid,1),(b,bid,-1)]:
            s.sendto(string('state')+struct.pack('!II',ident,seq)+struct.pack('=ff',direction[0]*x,direction[1]*x)+bytes([1,0,0,0,0,0,0])+struct.pack("=ff", 1, 0),server)
        seq+=1; next_send=now+1/64
    p=positions(recv_world(a))
    if p is None: continue
    left,right=p[aid],p[bid]
    if left is None or right is None: continue
    distance=math.dist(left,right)
    assert distance>=19.99,(left,right)
    if distance < 20.1:
        contact=True
        direction=((right[0]-left[0])/distance,(right[1]-left[1])/distance)
        break
assert contact,'Players never reached contact'
seq+=1
for sock,ident in [(a,aid),(b,bid)]:
    sock.sendto(string('state')+struct.pack('!II',ident,seq)+struct.pack('=ff',0,0)+bytes([1,0,0,0,0,0,0])+struct.pack('=ff',*direction),server)
print('PASS: separate spawns and two network clients sprinting head-on cannot overlap or pass through')
