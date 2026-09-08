"""Run against a fresh local server: python3 tests/collision_network.py."""
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
            position = struct.unpack_from('=ff', data, offset + 2)
            offset += 18
            size = struct.unpack_from('!I', data, offset)[0]
            offset += 4 + size + 31
            count=data[offset+4];offset+=5+count*20
    return position

# Demo spawn is (-140, 620); the fence crosses the northbound path near y=515.
x, y = drive(4.0 / 3.0, (0, -1))
assert x > -130 and y > 260, (x, y)  # blocked and redirected along the fence
before = drive(0.1, (0, 0))
after = drive(0.2, (0, 0))
assert abs(before[0]-after[0]) < 0.01 and abs(before[1]-after[1]) < 0.01
print('PASS: server movement hits the map fence, slides along it, and stops on release')
