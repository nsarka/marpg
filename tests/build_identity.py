"""Wire suffix for test clients joining the locally built server."""
import pathlib
import re
import struct
header=pathlib.Path(__file__).resolve().parent.parent/'build/generated/build_version.hpp'
commit=re.search(r'"([0-9a-f]{40})"',header.read_text())[1].encode()
join_identity=struct.pack('!II',0,len(commit))+commit
