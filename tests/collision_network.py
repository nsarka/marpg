"""Run against a fresh local server: python3 tests/collision_network.py."""
import os
from world_transport import recv_world
# Also exercises the existing attack protocol after the movement changes.
from attack_network import sock, server, string, player_id
import struct
import time

sequence = 4

def drive(seconds, move):
    global sequence
    end = time.monotonic() + seconds
    next_send = 0
    position = None
    while time.monotonic() < end:
        now = time.monotonic()
        if now >= next_send:
            sock.sendto(string('state') + struct.pack('!II', player_id, sequence)
                        + struct.pack('=ff', *move) + bytes([1, 0, 0, 0, 0, 0, 0]) + struct.pack("=ff", 1, 0), server)
            sequence += 1
            next_send = now + 1/64
        data = recv_world(sock)
        offset = 4 + struct.unpack_from('!I', data)[0]
        if data[4:offset] != b'world':
            continue
        for index in range(player_id + 1):
            offset += 4
            position = struct.unpack_from('=ff', data, offset + 2)
            offset += 18
            size = struct.unpack_from('!I', data, offset)[0]
            offset += 4 + size + 31
            count=data[offset+4];offset+=5+count*20
    return position

# In tests/network.toml, the third authored spawn has a clear approach to the window.
import math
start=drive(.1,(0,0));target=(948,694)
delta=(target[0]-start[0],target[1]-start[1]);length=math.hypot(*delta)
direction=(delta[0]/length,delta[1]/length)
end=drive(1,direction)
predicted=(start[0]+270*direction[0],start[1]+270*direction[1])
assert math.dist(end,predicted)>25,(start,end,predicted)
before = drive(0.1, (0, 0))
after = drive(0.2, (0, 0))
assert abs(before[0]-after[0]) < 0.01 and abs(before[1]-after[1]) < 0.01
print('PASS: server movement hits the map window, slides along it, and stops on release')
