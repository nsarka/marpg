from build_identity import join_identity
"""Run against a fresh local server: python3 tests/attack_network.py."""
import os
from world_transport import recv_world
import socket
import struct
import time


def string(value):
    data = value.encode()
    return struct.pack('!I', len(data)) + data


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2)
server = ('127.0.0.1', int(os.environ.get('MARPG_TEST_PORT','54000')))
sock.sendto(string('join') + string('Attack test') + join_identity, server)
while True:
    data = recv_world(sock)
    size = struct.unpack_from('!I', data)[0]
    if data[4:4 + size] == b'join_ack':
        player_id = struct.unpack_from('!i', data, 4 + size)[0]
        assert player_id >= 0
        break


def command(sequence, jab=False, hook=False):
    if jab or hook:
        return (string('attack') + struct.pack('!IIB', player_id, sequence, 1 if jab else 2)
                + struct.pack('=ff', 1, 0) + struct.pack('!I', 0))
    return (string('state') + struct.pack('!II', player_id, sequence)
            + struct.pack('=ff', 0, 0)
            + bytes([False, jab, jab, False, hook, hook, False]) + struct.pack("=ff", 1, 0))


def attack_state(data):
    offset = 4 + struct.unpack_from('!I', data)[0]
    if data[4:offset] != b'world':
        return None
    for index in range(player_id + 1):
        offset += 22  # connected, alive, position and velocity
        size = struct.unpack_from('!I', data, offset)[0]
        offset += 4 + size + 8  # name, health and score
        kind, serial = struct.unpack_from('!BI', data, offset)
        offset += 23
        count = data[offset + 4]
        offset += 5 + count * 20
    return kind, serial


def observe(expected, duration=0.2):
    deadline = time.monotonic() + duration
    latest = None
    while time.monotonic() < deadline:
        state = attack_state(recv_world(sock))
        if state is not None:
            latest = state
    assert latest == expected, (latest, expected)


sock.sendto(command(0, jab=True), server)
observe((1, 1))
# A second press during the clip must not interrupt it.
sock.sendto(command(1, hook=True), server)
observe((1, 1))
time.sleep(0.8)
sock.sendto(command(2, hook=True), server)
observe((2, 2))
time.sleep(1.1)
# Replayed inputs must never retrigger a completed swing.
sock.sendto(command(2, hook=True), server)
sock.sendto(command(0, jab=True), server)
observe((2, 2))
sock.sendto(command(3, jab=True), server)
observe((1, 3))
print('PASS: jab, hook, cooldown, repeated attacks, duplicate/out-of-order inputs, snapshot serialization')
