"""Run against a fresh demo server: python3 tests/reliable_attack_network.py."""
from world_transport import recv_world
import socket, struct, time

def string(value):
    b=value.encode();return struct.pack('!I',len(b))+b
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.settimeout(2)
server=('127.0.0.1',54000)
sock.sendto(string('join')+string('Reliable attack test'),server)
while True:
    data=recv_world(sock);n=struct.unpack_from('!I',data)[0]
    if data[4:4+n]==b'join_ack':
        ident=struct.unpack_from('!i',data,4+n)[0];assert ident>=0;break

acks=set();latest=0

def pump(duration):
    global latest
    end=time.monotonic()+duration
    while time.monotonic()<end:
        data=recv_world(sock);offset=4+struct.unpack_from('!I',data)[0];kind=data[4:offset]
        if kind==b'attack_ack':
            acks.add(struct.unpack_from('!I',data,offset)[0]);continue
        if kind!=b'world':continue
        for i in range(ident+1):
            offset+=18;n=struct.unpack_from('!I',data,offset)[0];offset+=4+n+8
            attack,latest=struct.unpack_from('!BI',data,offset);offset+=23
            count=data[offset+4];offset+=5+count*20
    return latest

def attack(sequence,kind=1,age=0):
    sock.sendto(string('attack')+struct.pack('!IIB',ident,sequence,kind)+struct.pack('=ff',1,0)+struct.pack('!I',age),server)

# A newer movement packet must not discard an independently sequenced attack.
sock.sendto(string('state')+struct.pack('!II',ident,99999)+struct.pack('=ff',0,0)
            +bytes([1,0,0,0,0,0,0])+struct.pack('=ff',1,0),server)
# Drop the first attempt; deliver the same logical request on its retry.
pump(0.05);attack(1,age=50)
assert pump(0.1)==1 and 1 in acks
# Pretend the first ACK was lost. Retry must be ACKed again without replay.
acks.clear();attack(1,age=150)
assert pump(0.1)==1 and 1 in acks
pump(0.6) # First attack is now about 0.8 seconds old.
attack(2,kind=2)
assert pump(0.28)==2 and 2 in acks,'Late click should start immediately after recovery'
attack(1,age=0) # Reordered old request, even with a fresh-looking age.
assert pump(1.0)==2,'Duplicate must never restart a finished swing'
attack(3,age=300)
assert pump(0.1)==2 and 3 in acks,'Expired inputs are ACKed and discarded'
attack(4)
assert pump(0.1)==3
attack(5,kind=2) # Too early: buffer expires well before recovery ends.
assert pump(1.0)==3 and 5 in acks,'Early buffer must expire without a surprise attack'
print('PASS: dropped request retry, lost ACK, deduplication, movement independence, late buffer, stale/early expiry')
