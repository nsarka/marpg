"""Run against a fresh demo server: python3 tests/player_collision_network.py."""
import socket, struct, time, math
server = ('127.0.0.1', 54000)
def string(s):
    b=s.encode(); return struct.pack('!I',len(b))+b

def connect(name):
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(2)
    s.sendto(string('join')+string(name),server)
    while True:
        b=s.recv(65535); n=struct.unpack_from('!I',b)[0]
        if b[4:4+n]==b'join_ack':
            ident=struct.unpack_from('!i',b,4+n)[0]; assert ident>=0
            return s,ident

a,aid=connect('Collision A'); b,bid=connect('Collision B')

def positions(data):
    offset=4+struct.unpack_from('!I',data)[0]
    if data[4:offset]!=b'world': return None
    result=[]
    for i in range(32):
        result.append(struct.unpack_from('=ff',data,offset+2) if data[offset] else None); offset+=18
        n=struct.unpack_from('!I',data,offset)[0]; offset+=4+n+31
        count=data[offset+4];offset+=5+count*20
    return result

start=time.monotonic(); next_send=0; seq=0; latest=None; contact=False
while time.monotonic()-start<1.5:
    now=time.monotonic()
    if now>=next_send:
        for s,ident,x in [(a,aid,1),(b,bid,-1)]:
            s.sendto(string('state')+struct.pack('!II',ident,seq)+struct.pack('=ff',x,0)+bytes([1,0,0,0,0,0,0])+struct.pack("=ff", 1, 0),server)
        seq+=1; next_send=now+1/64
    p=positions(a.recv(65535))
    if p is None: continue
    left,right=p[aid],p[bid]
    if left is None or right is None: continue
    distance=math.dist(left,right)
    assert distance>=19.99,(left,right)
    assert left[0]<right[0],(left,right)
    contact |= distance < 20.1
assert contact,'Players never reached contact'
print('PASS: separate spawns and two network clients sprinting head-on cannot overlap or pass through')
