"""The Pieces section of a LithTech ABC v12 model: read, change and write it back.

Layout (worked out from the game's files; round-trips byte for byte):
  u32 total vertex weights, u32 piece count, then per piece:
    u16 texture, f32 specular power, f32 specular scale, f32 LOD weight, u16 unknown,
    u16 name length + name,
    u32 triangle count, per triangle 3 x (f32 u, f32 v, u16 vertex index),
    u32 vertex count, per vertex: u16 weight count, u16 padding, per weight (u32 node, 3 f32, f32 weight),
      then 3 f32 position, 3 f32 normal
"""
import struct


class Piece:
    pass


def read_pieces(data):
    total, n = struct.unpack_from('<II', data, 0)
    p = 8
    pieces = []
    for _ in range(n):
        pc = Piece()
        pc.head = data[p:p + 16]  # texture, specular power/scale, LOD weight, unknown
        p += 16
        ln = struct.unpack_from('<H', data, p)[0]
        pc.name = data[p + 2:p + 2 + ln].decode('latin1')
        p += 2 + ln
        nt = struct.unpack_from('<I', data, p)[0]
        p += 4
        pc.tris = []
        for _ in range(nt):
            tri = []
            for _ in range(3):
                u, v, i = struct.unpack_from('<ffH', data, p)
                p += 10
                tri.append((u, v, i))
            pc.tris.append(tri)
        nv = struct.unpack_from('<I', data, p)[0]
        p += 4
        pc.verts = []
        for _ in range(nv):
            nw, pad = struct.unpack_from('<HH', data, p)
            p += 4
            weights = []
            for _ in range(nw):
                weights.append(data[p:p + 20])
                p += 20
            rest = data[p:p + 24]
            p += 24
            pc.verts.append((pad, weights, rest))
        pieces.append(pc)
    assert p == len(data), (p, len(data))
    return total, pieces


def weight_node(w):
    return struct.unpack_from('<I', w, 0)[0]


def write_pieces(pieces):
    out = bytearray()
    total = sum(len(v[1]) for pc in pieces for v in pc.verts)
    out += struct.pack('<II', total, len(pieces))
    for pc in pieces:
        out += pc.head
        nb = pc.name.encode('latin1')
        out += struct.pack('<H', len(nb)) + nb
        out += struct.pack('<I', len(pc.tris))
        for tri in pc.tris:
            for u, v, i in tri:
                out += struct.pack('<ffH', u, v, i)
        out += struct.pack('<I', len(pc.verts))
        for pad, weights, rest in pc.verts:
            out += struct.pack('<HH', len(weights), pad)
            for w in weights:
                out += w
            out += rest
    return bytes(out), total
