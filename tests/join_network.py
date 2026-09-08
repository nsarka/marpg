"""Run against a fresh server: python3 tests/join_network.py."""
import os
from world_transport import recv_world
import socket
import struct
import time


def string(value):
    data = value.encode()
    return struct.pack('!I', len(data)) + data


def receive_ack(sock, timeout=2):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        sock.settimeout(max(.001, deadline - time.monotonic()))
        try:
            data = recv_world(sock)
        except socket.timeout:
            return None
        size = struct.unpack_from('!I', data)[0]
        if data[4:4 + size] == b'join_ack':
            return struct.unpack_from('!i', data, 4 + size)[0]
    return None


sockets = []
try:
    for index in range(31):  # 30 human slots plus two bots, then rejection.
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sockets.append(sock)
        request = string('join') + string('Handshake test ' + str(index))
        sock.sendto(request, ('127.0.0.1', int(os.environ.get('MARPG_TEST_PORT','54000'))))
        player_id = receive_ack(sock)
        if index == 30:
            assert player_id == -1, player_id
            break
        assert player_id == index + 2, player_id
        # Simulate a lost acknowledgement by retrying the same request.
        sock.sendto(request, ('127.0.0.1', int(os.environ.get('MARPG_TEST_PORT','54000'))))
        assert receive_ack(sock) == player_id, 'Retry allocated a different player'
        if index:
            assert receive_ack(sockets[0], .05) is None, 'Assignment leaked to another player'
    print('PASS: join retry is idempotent; acknowledgements are private; full-server rejection arrives')
finally:
    for sock in sockets:
        sock.close()
