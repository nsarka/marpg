"""Verify shutdown notifications, retries to peers, and abrupt-loss fallback."""
import pathlib
import signal
import socket
import struct
import subprocess
import tempfile
import time
root=pathlib.Path(__file__).resolve().parent.parent

def string(value):
    data=value.encode();return struct.pack('!I',len(data))+data

def receive(sock,kind):
    deadline=time.monotonic()+3
    while time.monotonic()<deadline:
        data=sock.recv(65535);n=struct.unpack_from('!I',data)[0]
        if data[4:4+n]==kind.encode():return data[4+n:]
    raise AssertionError('No '+kind)

for stop in (signal.SIGINT,signal.SIGTERM,signal.SIGKILL):
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix='marpg-shutdown-') as tmp:
        folder=pathlib.Path(tmp);(folder/'build').mkdir();(folder/'assets').symlink_to(root/'assets',target_is_directory=True)
        (folder/'server.toml').write_text(f'[server]\nip="127.0.0.1"\nport={port}\nslots=6\nteams=2\nbots=0\n')
        with open(folder/'server.log','w') as log,open(folder/'client.log','w') as clientlog:
            server=subprocess.Popen([str(root/'build/server')],cwd=folder/'build',stdout=log,stderr=log)
            client=None;peers=[]
            try:
                deadline=time.monotonic()+15
                while 'Server listening on port' not in (folder/'server.log').read_text():
                    assert server.poll() is None and time.monotonic()<deadline
                    time.sleep(.02)
                client=subprocess.Popen([str(root/'build/shutdown_probe'),str(port),'lost' if stop==signal.SIGKILL else 'notice'],stdout=clientlog,stderr=clientlog)
                while 'READY' not in (folder/'client.log').read_text():
                    assert client.poll() is None and time.monotonic()<deadline
                    time.sleep(.02)
                for i in range(2):
                    peer=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);peer.settimeout(3);peers.append(peer)
                    peer.sendto(string('join')+string(f'Peer {i}'),('127.0.0.1',port))
                    ident=struct.unpack_from('!i',receive(peer,'join_ack'))[0]
                    assert ident==i+1
                server.send_signal(stop)
                if stop!=signal.SIGKILL:
                    for i,peer in enumerate(peers):
                        receive(peer,'server_shutdown') # Lose first notice/ack, then receive retry.
                        receive(peer,'server_shutdown')
                        peer.sendto(string('shutdown_ack')+struct.pack('!I',i+1),('127.0.0.1',port))
                server.wait(timeout=3)
                assert client.wait(timeout=14)==0,(folder/'client.log').read_text()
                print(f'PASS: {stop.name}: client shutdown state; '+('all peers notified with retries' if stop!=signal.SIGKILL else 'connection-loss timeout'))
            finally:
                for peer in peers:peer.close()
                if client and client.poll() is None:client.kill();client.wait()
                if server.poll() is None:server.kill();server.wait()
