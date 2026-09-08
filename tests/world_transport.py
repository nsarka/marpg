"""Decode the game's small world datagrams for network regression tests."""
import struct
import weakref

_pending = weakref.WeakKeyDictionary()

def recv_world(sock, size=65535):
    while True:
        data=sock.recv(size)
        if len(data)<4: continue
        length=struct.unpack_from('!I',data)[0]
        if data[4:4+length]!=b'world_part': return data
        offset=4+length
        sequence,index,count,total=struct.unpack_from('!IHHI',data,offset)
        payload=data[offset+12:]
        if not 0<total<=65536 or count!=(total+999)//1000 or index>=count: continue
        if len(payload)!=min(1000,total-index*1000): continue
        pending=_pending.setdefault(sock,{})
        if sequence not in pending:
            if len(pending)>=4: pending.pop(next(iter(pending)))
            pending[sequence]={}
        parts=pending[sequence];parts[index]=payload
        if len(parts)==count:
            del pending[sequence]
            return b''.join(parts[i] for i in range(count))
