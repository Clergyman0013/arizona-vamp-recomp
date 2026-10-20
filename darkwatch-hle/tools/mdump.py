"""Minimal minidump reader: exception record, crashing thread context, module list.
usage: python mdump.py <dump> [stack_qwords]"""
import struct, sys

data = open(sys.argv[1], 'rb').read()
nq = int(sys.argv[2]) if len(sys.argv) > 2 else 0
sig, ver, nstreams, dir_rva = struct.unpack_from('<IIII', data, 0)
streams = {}
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', data, dir_rva + 12 * i)
    streams[t] = (size, rva)

def utf16(rva):
    n = struct.unpack_from('<I', data, rva)[0]
    return data[rva + 4:rva + 4 + n].decode('utf-16-le')

mods = []
size, rva = streams[4]
n = struct.unpack_from('<I', data, rva)[0]
for i in range(n):
    m = rva + 4 + 108 * i
    base, sz = struct.unpack_from('<QI', data, m)
    name_rva = struct.unpack_from('<I', data, m + 20)[0]
    mods.append((base, sz, utf16(name_rva)))

def modof(a):
    for b, s, nm in mods:
        if b <= a < b + s:
            return nm.split('\\')[-1], a - b
    return None, None

size, rva = streams[6]
tid = struct.unpack_from('<I', data, rva)[0]
code, flags, rec, addr = struct.unpack_from('<IIQQ', data, rva + 8)
ctx_size, ctx_rva = struct.unpack_from('<II', data, rva + 8 + 152)
print(f'thread {tid:#x} code {code:#x} addr {addr:#x} {modof(addr)}')
# CONTEXT (AMD64): Rsp at 0x98, Rip at 0xF8
rsp = struct.unpack_from('<Q', data, ctx_rva + 0x98)[0]
rip = struct.unpack_from('<Q', data, ctx_rva + 0xF8)[0]
print(f'rip {rip:#x} {modof(rip)}  rsp {rsp:#x}')
for b, s, nm in mods:
    if nm.lower().endswith('darkwatch.exe'):
        print(f'exe base {b:#x} size {s:#x}')

# the host stack from the memory list: Memory64ListStream (9) or MemoryListStream (5)
ranges = []  # (start, size, file offset)
if 9 in streams:
    size, rva = streams[9]
    cnt, off = struct.unpack_from('<QQ', data, rva)
    for i in range(cnt):
        start, sz = struct.unpack_from('<QQ', data, rva + 16 + 16 * i)
        ranges.append((start, sz, off))
        off += sz
elif 5 in streams:
    size, rva = streams[5]
    cnt = struct.unpack_from('<I', data, rva)[0]
    for i in range(cnt):
        start, sz, mrva = struct.unpack_from('<QII', data, rva + 4 + 16 * i)
        ranges.append((start, sz, mrva))
for start, sz, off in ranges if nq else []:
    if start <= rsp < start + sz:
        p = off + (rsp - start)
        for k in range(min(nq, (start + sz - rsp) // 8)):
            v = struct.unpack_from('<Q', data, p + 8 * k)[0]
            nm, o = modof(v)
            if nm and nm.lower() == 'darkwatch.exe':
                print(f'  [rsp+{8*k:#x}] {nm}+{o:#x}')
        break
