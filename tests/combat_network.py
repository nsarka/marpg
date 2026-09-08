"""Run against a fresh demo server: python3 tests/combat_network.py."""
from world_transport import recv_world
# Establishes two facing players at contact and checks player collision first.
from player_collision_network import a, b, aid, bid, seq, string, server
import struct, time


def send(s, ident, jab=False, hook=False):
    global seq
    seq += 1
    if jab or hook:
        s.sendto(string('attack')+struct.pack('!IIB',ident,seq,1 if jab else 2)
                 +struct.pack('=ff',1,0)+struct.pack('!I',0),server)
    s.sendto(string('state')+struct.pack('!II',ident,seq)+struct.pack('=ff',0,0)
             +bytes([1,jab,jab,0,hook,hook,0])+struct.pack("=ff", 1, 0),server)


def victim(data):
    offset=4+struct.unpack_from('!I',data)[0]
    if data[4:offset]!=b'world': return None
    for i in range(bid+1):
        connected, alive=data[offset],data[offset+1]
        position=struct.unpack_from('=ff',data,offset+2)
        offset+=18
        n=struct.unpack_from('!I',data,offset)[0];offset+=4+n
        health=struct.unpack_from('!i',data,offset)[0];offset+=31
        count=data[offset+4];offset+=5
        events=[]
        for event in range(count):
            event_id,amount,source=struct.unpack_from('!Iii',data,offset)
            contact=struct.unpack_from('=ff',data,offset+12)
            events.append((event_id,amount,source,contact));offset+=20
    return bool(alive),health,position,events

send(a,aid);send(b,bid)
previous_health=100
for swing in range(4):
    send(a,aid,jab=swing==0,hook=swing!=0)
    end=time.monotonic()+1.1
    state=None
    while time.monotonic()<end:
        got=victim(recv_world(a))
        if got: state=got
    expected=max(0,80-35*swing)
    assert state and state[1]==expected,(swing,state,expected)
    assert state[3][-1][1]==previous_health-expected and state[3][-1][2]==aid, "Damage event amount/source mismatch"
    previous_health=expected
assert state[0] is False and state[1]==0,'Victim must die at zero health'
death_position=state[2]
end=time.monotonic()+3
while time.monotonic()<end:
    state=victim(recv_world(a))
    if state and state[0]:
        assert state[1]==100,'Respawn health must be full'
        assert state[2]!=death_position,'Respawn should use a free spawn'
        print('PASS: network jab/hook damage, lethal hit, death snapshot, full-health respawn')
        break
else:
    raise AssertionError('No automatic respawn')
